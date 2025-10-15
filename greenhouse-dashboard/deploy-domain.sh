#!/bin/bash
# Deploy script para reimon.dev/greenhouse

set -e

echo "🔨 Building frontend for production..."
npm run build

echo "📦 Deploying to VPS (168.181.185.42)..."
rsync -avz --delete \
  --progress \
  dist/ \
  root@168.181.185.42:/var/www/greenhouse/

echo ""
echo "✅ Deploy completed successfully!"
echo "🌐 Visit: https://reimon.dev/greenhouse"
echo ""
echo "📋 Verify deployment:"
echo "  - Frontend: curl -I https://reimon.dev/greenhouse"
echo "  - API: curl https://reimon.dev/greenhouse/api/health"
