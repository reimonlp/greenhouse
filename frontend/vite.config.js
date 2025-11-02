import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  base: process.env.NODE_ENV === 'production' ? '/greenhouse/' : '/', // Production uses /greenhouse/, dev uses /
  server: {
    port: 5173,
    host: true
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    // Sin manualChunks: dejar que Vite/Rollup gestione los chunks automáticamente
  }
})
