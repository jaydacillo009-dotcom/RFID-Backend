# RFID equipment borrowing with Supabase

The ESP32 scans the student card and item tag and sends requests over its USB cable to a laptop. `laptop_bridge.py` signs and forwards those requests over HTTPS to the `rfid-api` service, hosted on Supabase Edge Functions or Render. The laptop needs internet access; the ESP32 does not. The service checks registrations and records loans in Supabase Postgres. A browser kiosk subscribes to the public process state; it never receives a card UID or device secret.

The project from the dashboard screenshot is `edelmgnzwcngnozaxrsx`. Its API base URL is:

```text
https://edelmgnzwcngnozaxrsx.supabase.co/functions/v1/rfid-api
```

## Deploy the database and function

Install the [Supabase CLI](https://supabase.com/docs/guides/local-development/cli/getting-started), then run from this repository:

```sh
supabase login
supabase link --project-ref edelmgnzwcngnozaxrsx
supabase db push --dry-run
supabase db push
supabase functions deploy rfid-api --use-api
```

`supabase/migrations/20261014000000_create_rfid_schema.sql` creates the tables and enables Realtime on `device_state`. `supabase/config.toml` sets `verify_jwt = false` because laptop bridge requests use device HMAC credentials rather than Supabase JWTs. The function verifies each signed POST and allows public reads only for health, Realtime configuration, and process status. Supabase injects `SUPABASE_DB_URL` into the function; no database URL or privileged key belongs in the frontend or firmware. Only the non-sensitive `device_state` table grants anonymous read access. All other tables have row level security enabled without browser-facing policies.

Do not deploy the function before applying the migration. If your project already has RFID tables or data, inspect `supabase db push --dry-run` first and reconcile them before applying the migration.

## Host the API on Render

Render runs the same Deno API in `supabase/functions/rfid-api/`; the database and Realtime subscription remain in Supabase. Apply the Supabase migration above before creating the Render service. You do not need to deploy the Edge Function when using Render.

1. In the Supabase dashboard, select **Connect → Session pooler** and copy the Postgres connection string. Replace its password placeholder with your database password, URL-encoding special characters. Add `?sslmode=require` if the URL has no query string, or `&sslmode=require` if it already has one. Use this as `SUPABASE_DB_URL`; keep it secret. The session pooler supports Render's IPv4 connections and a persistent server process.
2. Get the project's publishable key from Supabase **Project Settings → API Keys**. The key is public, but keep the database URL private.
3. Push this repository to GitHub, then create a **Blueprint** in Render from the repository. `render.yaml` defines a Docker web service. Enter `SUPABASE_DB_URL` and `SUPABASE_PUBLISHABLE_KEY` when Render asks. `SUPABASE_URL` is already set to this project's public URL; change it in `render.yaml` if deploying another Supabase project.
4. Once deployed, visit `https://<your-render-host>.onrender.com/api/v1/health`. It should return `{"ok":true}`. Set the laptop bridge's `RFID_API_BASE_URL` to `https://<your-render-host>.onrender.com` with no `/rfid-api` suffix. If a kiosk calls this API, update its base URL too; the `/api/v1/config` response still points it to Supabase Realtime.

The Blueprint uses an always-on Render web service. Render's free web service can take about a minute to wake after idle, longer than the scanner's 12-second response window. Render terminates HTTPS at the edge, and the Deno process listens on Render's `PORT` over HTTP inside the service. The Docker image does not contain database credentials.

## Enroll a scanner, student, and item

Generate a unique 32-byte scanner secret locally:

```sh
python3 -c 'import secrets; print(secrets.token_hex(32))'
```

Use Supabase SQL Editor to run the following, substituting your generated secret and real scanned UIDs. Keep the secret private. UIDs must be uppercase hex with 8, 14, or 20 characters.

```sql
insert into public.devices(id, secret) values ('scanner-1', '<64 lowercase hex characters>');
insert into public.students(uid, name) values ('A1B2C3D4', 'Alice Santos');
insert into public.items(uid, name) values ('01020304', 'Science Book');
```

The device ID and secret must match the laptop bridge environment variables below. Keep the secret on the laptop; do not put it in ESP32 firmware, a `VITE_` variable, or Git. To revoke a scanner, set `public.devices.active = false` for its ID. The old Python `add-device`, `add-student`, and `add-item` commands write to a local SQLite file and do **not** enroll anything in Supabase.

## Wire the scanner to the laptop

Connect the ESP32 DevKit's USB data port to the laptop with a **data-capable USB cable**. USB supplies power and carries serial data; no extra ESP32-to-laptop signal wires are needed. Keep the components wired to the ESP32 as follows:

| Component | ESP32 connection |
| --- | --- |
| MFRC522 VCC / GND | 3.3 V / GND |
| MFRC522 SDA/SS, RST, SCK, MISO, MOSI | GPIO 5, 4, 18, 19, 23 |
| LCD SDA / SCL | GPIO 21 / 22; connect VCC and GND according to the backpack's voltage rating |
| Scan button / Menu button | GPIO 25 / GPIO 26, each button's other side to GND |
| Active buzzer signal | GPIO 27; connect module VCC and GND according to its rating |
| Green LED | GPIO 32 → 330 Ω resistor → LED anode (+); LED cathode (−) → GND |
| Red LED | GPIO 33 → 330 Ω resistor → LED anode (+); LED cathode (−) → GND |

Share ESP32 GND with the RFID module, LCD, and buzzer. Use `0x27` as the LCD I²C address or change `LCD_ADDRESS` in `arduino/RFIDScanner/RFIDScanner.h`. Check your LCD backpack voltage: if it needs 5 V, use an I²C level shifter so its SDA/SCL lines do not apply 5 V to the ESP32. Power the MFRC522 at **3.3 V**.

## Flash with Arduino IDE

For a simple LCD-only check, open `arduino/LCDTextTest/LCDTextTest.ino` in Arduino IDE and upload it to the **ESP32 Dev Module**. With LCD SDA on GPIO 21 and SCL on GPIO 22, the display should show `Hello, world!` and `LCD is working`. This sketch only needs the **LiquidCrystal_I2C** library. If the backlight turns on but the text is invisible, adjust the small contrast screw on the LCD backpack; if nothing appears, check the wiring and try address `0x3F` in the sketch.

To test the RFID reader and 16×2 I²C LCD without the laptop bridge, open `arduino/RFIDHardwareTest/RFIDHardwareTest.ino` in Arduino IDE and upload it to an **ESP32 Dev Module**. Install the **MFRC522** and **LiquidCrystal_I2C** libraries listed below. The LCD should show `RFID ready / Tap a card`; tapping a card displays its UID, and the Serial Monitor at **115200 baud** prints the full UID. A 10-byte UID spans both LCD rows. If the display stays blank, check its contrast adjustment, wiring, and I²C address (`0x27` in the test sketch; some backpacks use `0x3F`). If the LCD shows `RFID not found`, check the MFRC522 power and SPI wiring. Upload the main `RFIDScanner.ino` again after testing to restore the borrowing workflow.

Open `arduino/RFIDScanner/RFIDScanner.ino` in Arduino IDE. Keep `RFIDScanner.h` in the same folder; it contains the firmware used by both Arduino IDE and PlatformIO. In Arduino IDE, add Espressif's stable board manager URL `https://espressif.github.io/arduino-esp32/package_esp32_index.json` under **Preferences → Additional Boards Manager URLs**, then install **esp32 by Espressif Systems** in Boards Manager. Select **ESP32 Dev Module** and the serial port for your board. See [Espressif's Arduino IDE installation guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).

Install these libraries through Arduino IDE's Library Manager: **MFRC522** version 1.4.12, **LiquidCrystal_I2C** by Frank de Brabander / Marco Schwartz version 1.1.4, and **ArduinoJson** version 7.x. Then click **Verify** and **Upload**. The laptop bridge must be stopped while Arduino IDE owns the serial port; restart it after upload. [Arduino's library installation guide](https://support.arduino.cc/hc/en-us/articles/5145457742236-Install-libraries-in-the-Arduino-IDE) explains where to find Library Manager.

## Install the laptop and firmware tools

Install Python 3.9 or newer first, then run the same installer script on macOS, Linux, or Windows. It creates a project-local `.venv`, installs PySerial and PlatformIO, and downloads the ESP32 platform, toolchain, framework, and libraries declared in `esp32/platformio.ini`. It needs internet access during installation.

| System | Command from repository root |
| --- | --- |
| macOS or Linux | `python3 install.py` |
| Windows PowerShell or Command Prompt | `py -3 install.py` |

The script can be rerun. Use `python3 install.py --dry-run` (or `py -3 install.py --dry-run` on Windows) to preview its commands. If Linux reports that it cannot create a virtual environment, install your distribution's Python `venv` package. USB serial access also depends on your board's USB driver and OS permissions: Windows may need the board manufacturer's driver; Linux may need serial device permissions or PlatformIO udev rules. See [PlatformIO's system requirements](https://docs.platformio.org/en/latest/core/installation/requirements.html).

Flash the ESP32 with the installed PlatformIO executable:

```sh
.venv/bin/pio run --project-dir esp32 -t upload
```

On Windows, use `.venv\Scripts\pio.exe run --project-dir esp32 -t upload` instead. To find the port, run `.venv/bin/pio device list` on macOS/Linux or `.venv\Scripts\pio.exe device list` on Windows.

From the repository root, set the values from your Supabase project. Replace the example port with your actual port (Linux often `/dev/ttyUSB0` or `/dev/ttyACM0`; macOS often `/dev/cu.*`; Windows uses `COM3` or similar):

```sh
export RFID_API_BASE_URL='https://edelmgnzwcngnozaxrsx.supabase.co/functions/v1/rfid-api' # or your Render base URL
export RFID_DEVICE_ID='scanner-1'
export RFID_DEVICE_SECRET_HEX='<the 64-character scanner secret>'
.venv/bin/python laptop_bridge.py --port /dev/ttyUSB0
```

On Windows PowerShell, set the same variables with `$env:RFID_API_BASE_URL='...'`, `$env:RFID_DEVICE_ID='...'`, and `$env:RFID_DEVICE_SECRET_HEX='...'`, then run `.venv\Scripts\python.exe laptop_bridge.py --port COM3`. Keep the bridge running while using the scanner. Close the PlatformIO device monitor before starting it; both programs cannot own the same serial port. The bridge prints scanner diagnostics and API status without printing the secret.

Menu chooses BORROW or RETURN while idle. Press Scan, present the student card, present the item tag, then press Scan to confirm the displayed names. Menu cancels an unfinished session. The function rejects unknown/inactive cards and items, already borrowed items, and returns by the wrong student. A repeated confirmation request returns its original result without creating a second loan.

The green LED turns on after the API confirms a saved borrow or return. The red LED turns on for a rejected card/tag or failed API request. Both LEDs turn off when the next scan or button action starts; the last result stays visible until then. Use a separate 330 Ω resistor for each LED. These connections are for ordinary two-leg LEDs, not LED modules with built-in resistors.

If the laptop or API is unavailable, the LCD shows an error. Press Scan to retry a confirmation after restoring the connection; the same request ID prevents a duplicate record. The ESP32 does not queue scans while the laptop is disconnected.

## API

All paths below are appended to the API base URL above. The logical path, such as `/api/v1/preview`, is also the path used in the signature string.

| Method and path | Purpose |
| --- | --- |
| `GET /api/v1/health` | Function health |
| `GET /api/v1/config` | Public Supabase URL and publishable key for Realtime |
| `GET /api/v1/state?device_id=scanner-1` | Read-only mode/stage for the kiosk |
| `POST /api/v1/state` | Signed device process update |
| `POST /api/v1/preview` | Signed student/item validation |
| `POST /api/v1/commit` | Signed, idempotent loan or return |

The laptop bridge adds `X-Device-Id`, `X-Timestamp` (Unix seconds), `X-Nonce` (32 lowercase hex characters), and `X-Signature` (HMAC-SHA256 lowercase hex) to each POST. It signs these UTF-8 bytes using the scanner's 32-byte secret:

```text
POST\n<LOGICAL_PATH>\n<TIMESTAMP>\n<NONCE>\n<SHA256_HEX_OF_EXACT_JSON_BODY>
```

The laptop clock must be within five minutes of the server. Nonces cannot be replayed. `preview` accepts `{"mode":"borrow","student_uid":"A1B2C3D4","item_uid":null}` after the student scan, then the same body with the item UID. `commit` also requires a random 32-character lowercase hex `request_id`. `state` accepts a mode (`borrow` or `return`) and stage (`idle`, `scan_student`, `scan_item`, `confirm`, `success`, or `error`). The public state response includes no student or item UID.

The kiosk subscribes to Postgres changes for `device_state` and refreshes state once a minute. This keeps process updates immediate without constant Edge Function polling. The `config` route returns a **publishable** Supabase key, never a privileged database key.

## Checks

```sh
deno check supabase/functions/rfid-api/index.ts
deno lint supabase/functions/rfid-api/index.ts
deno run -A supabase/tests/schema_test.ts
```

**Card security limit:** MFRC522 reads a card UID, which can be copied or emulated. A registered UID and signed scanner request do not prove the cardholder's identity.

## Legacy local server

`rfid_backend/` and `tests/test_backend.py` remain for the earlier standalone Python/SQLite setup and its tests. They are separate from the Supabase deployment. Do not run both backends against the same scanner configuration.
