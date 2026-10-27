FROM docker.io/denoland/deno:2.9.5

WORKDIR /app
COPY --chown=deno:deno supabase/functions/rfid-api/ ./
USER deno
RUN deno cache server.ts

EXPOSE 10000
CMD ["deno", "run", "--allow-env=PORT,SUPABASE_DB_URL,SUPABASE_URL,SUPABASE_ANON_KEY,SUPABASE_PUBLISHABLE_KEY,SUPABASE_PUBLISHABLE_KEYS", "--allow-net", "server.ts"]
