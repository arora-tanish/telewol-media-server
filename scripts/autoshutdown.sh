#!/bin/bash

# --- CONFIGURATION ---
QBIT_URL="http://127.0.0.1:8585"
QBIT_USER="admin"
QBIT_PASS="qbit_pass"  # <-- Be sure to type your actual qBittorrent password here
CHECK_FILE="/tmp/server_idle_minutes"
LOG_FILE="/var/log/autoshutdown.log"
JELLYFIN_URL="http://127.0.0.1:8097"
JELLYFIN_KEY="jellyfin_api_key_here"  # <-- Be sure to type your actual Jellyfin API key here
TG_TOKEN="telegram_bot_token_here"  # <-- Be sure to type your actual Telegram Bot API token here
TG_CHAT_ID="telegram_chat_id_here"  # <-- Be sure to type your actual Telegram chat ID here 
# ---------------------

# Ensure log file exists
touch "$LOG_FILE" 2>/dev/null

log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> "$LOG_FILE"
}

# 1. Check Jellyfin Streaming via API (Looks for active playing sessions)
JELLYFIN_ACTIVE=""
JELLYFIN_SESSIONS=$(curl -s -H "X-MediaBrowser-Token: $JELLYFIN_KEY" "$JELLYFIN_URL/Sessions")
if echo "$JELLYFIN_SESSIONS" | grep -q '"NowPlayingItem"'; then
    JELLYFIN_ACTIVE="yes"
fi

# 2. Check CasaOS Web Interface Activity (Port 81 or 80 from external devices only)
# Looks for established external connections, completely ignoring internal localhost traffic (127.0.0.1 / ::1)
CASAOS_ACTIVE=$(netstat -anp 2>/dev/null | grep -E ' (192\.168\.|10\.|172\.(1[6-9]|2[0-9]|3[0-1])\.)[0-9.]*:(81|80) ' | grep ESTABLISHED)

# 3. Check Active Samba File Transfers (Port 445 from ANY external device)
# This sniffs port 445 for a maximum of 10 seconds (-G 10). It excludes localhost (127.0.0.1) 
# and looks for at least 150 packets to confirm an active data upload/download is happening.
SAMBA_ACTIVE=""
if timeout 10 tcpdump -i any -nqc 150 port 445 and not host 127.0.0.1 -w /dev/null 2>&1; then
    SAMBA_ACTIVE="yes"
fi

# 4. Check qBittorrent Active States (API Login)
COOKIE_JAR="/tmp/qbit_cookies.txt"
rm -f "$COOKIE_JAR"

# Log into qBittorrent API securely
curl -s -c "$COOKIE_JAR" --header "Referer: $QBIT_URL" --data "username=$QBIT_USER&password=$QBIT_PASS" "$QBIT_URL/api/v2/auth/login" > /dev/null

# Pull the state text of all torrents currently in your queue
TORRENT_STATES=$(curl -s -b "$COOKIE_JAR" "$QBIT_URL/api/v2/torrents/info" | grep -o '"state":"[^"]*"' | cut -d'"' -f4)
rm -f "$COOKIE_JAR"

# Instead of checking erratic speeds, check if any torrent is fundamentally uncompleted/active
if echo "$TORRENT_STATES" | grep -qE "downloading|stalledDL|checkingDL|allocating|forcedDL"; then
    QBIT_ACTIVE="yes"
else
    QBIT_ACTIVE=""
fi


# --- DECISION LOGIC ---
if [ -n "$JELLYFIN_ACTIVE" ] || [ -n "$CASAOS_ACTIVE" ] || [ -n "$SAMBA_ACTIVE" ] || [ -n "$QBIT_ACTIVE" ]; then
    # Server is busy doing something! Clear the tracking clock.
    echo "0" > "$CHECK_FILE"
    echo "Server is busy. Resetting idle countdown timer."
    
    # Identify what is keeping it awake for the log file
    REASON=""
    [ -n "$JELLYFIN_ACTIVE" ] && REASON+="[Jellyfin] "
    [ -n "$CASAOS_ACTIVE" ] && REASON+="[CasaOS Dashboard] "
    [ -n "$SAMBA_ACTIVE" ] && REASON+="[Active SMB Transfer] "
    [ -n "$QBIT_ACTIVE" ] && REASON+="[qBittorrent Active] "
    log_message "Server is busy. Timer reset. Active services: $REASON"
else
    # Server is truly dead quiet. Begin the 30-minute countdown.
    if [ ! -f "$CHECK_FILE" ]; then
        echo "0" > "$CHECK_FILE"
    fi

    CURRENT_IDLE=$(cat "$CHECK_FILE")
    NEW_IDLE=$((CURRENT_IDLE + 5))
    echo "$NEW_IDLE" > "$CHECK_FILE"
    echo "Server is idle. Accumulating idle time: $NEW_IDLE minutes."
    log_message "Server is idle. Accumulated idle time: $NEW_IDLE minutes."

    if [ "$NEW_IDLE" -ge 30 ]; then
        echo "30 continuous minutes of complete silence reached. Safe powerdown."
        log_message "30 minutes of continuous idle reached. Initiating system shutdown."
        # Send a notification to your phone via the Telegram Bot API
        MESSAGE="⚠️ Server notice: 30 minutes of continuous inactivity reached. The system is shutting down now."
        curl -s -X POST "https://api.telegram.org/bot$TG_TOKEN/sendMessage" \
         -d "chat_id=$TG_CHAT_ID" \
         -d "text=$MESSAGE" > /dev/null
        rm -f "$CHECK_FILE"
        # Gracefully stop docker containers so they release the drive
        sudo systemctl stop docker

        # Explicitly unmount the HDD safely before the OS cuts power
        sudo umount /mnt/HDD
        sudo umount /mnt/LHDD

        # Now safely shut down the system hardware
        sudo shutdown -h now
    fi
fi
