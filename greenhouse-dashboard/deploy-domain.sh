#!/bin/bash
# Deploy script para reimon.dev/greenhouse

set -e

echo "🔨 Building frontend for production..."
npm run build

echo "📦 Deploying to VPS (reimon.dev)..."
rsync -avz --delete \
  --progress \
  dist/ \
  root@reimon.dev:/var/www/greenhouse/

echo ""
echo "✅ Deploy completed successfully!"
echo "🌐 Visit: https://reimon.dev/greenhouse"
echo ""
echo "📋 Verify deployment:"
echo "  - Frontend: curl -I https://reimon.dev/greenhouse"
echo "  - API: curl https://reimon.dev/greenhouse/api/health"
