import api from './index.ts'

const port = Number(Deno.env.get('PORT') ?? '10000')
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error('PORT must be a number between 1 and 65535')
}

Deno.serve({ hostname: '0.0.0.0', port }, api.fetch)
