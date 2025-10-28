import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  base: '/greenhouse/', // Base path para el dominio reimon.dev/greenhouse
  server: {
    port: 5173,
    host: true
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    rollupOptions: {
      output: {
        manualChunks(id) {
          if (id.includes('node_modules')) {
            if (id.includes('react')) return 'react';
            if (id.includes('recharts')) return 'recharts';
            if (id.includes('mui')) return 'mui';
            return 'vendor';
          }
        }
      }
    }
  }
})
