#!/bin/bash

# CONFIGURATION
TG_TOKEN="YOUR_TELEGRAM_BOT_TOKEN"
TG_CHAT_ID="YOUR_TELEGRAM_CHAT_ID"
MESSAGE="🚀 Server Notice: System has been powered on/woken up successfully and is now online!"

# Wait briefly for network interfaces to fully clear and grab an IP address
sleep 5

# Send message to Telegram
curl -s -X POST "https://api.telegram.org/bot$TG_TOKEN/sendMessage" \
     -d "chat_id=$TG_CHAT_ID" \
     -d "text=$MESSAGE" > /dev/null
