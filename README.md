# telewol-media-server
An on-demand, zero-standby Home Server (NAS/Media) running on Linux. Features a custom hardware hack tapping an ATX PSU standby rail (+5VSB) to power an ESP8266 NodeMCU, enabling secure Telegram-controlled Wake-on-LAN. Containerized via Docker (Jellyfin, Samba, Tailscale exit node) with automated bash-monitored idle power down.
