# TeleWoL Media Server: On-Demand Personal Backup & Media Server

Welcome to **TeleWoL Media Server**, a power-efficient, on-demand home server architecture designed to run on a spare dual-core machine. This setup prioritizes system integration, Linux power management, security, and NAS architecture.

---

## 📂 Repository Structure

Here is the directory layout for the server configuration and hardware triggers:

```text
telewol-media-server/
├── .gitignore                    # Prevents uploading credentials, tokens, and logs
├── README.md                     # Main project documentation and installation guide
├── hardware-switch-guide.md      # ESP8266 firmware code and ATX wire tapping guide
├── docker-compose.yml            # Container stack definition (Jellyfin, Samba, Tailscale)
├── config/
│   └── samba/
│       └── smb.conf              # Samba reference configuration
├── scripts/
│   ├── enable-wol.sh             # Wake-on-LAN persistence configuration script
│   └── autosuspend.sh            # Idle monitoring daemon script
└── systemd/
    ├── wol.service               # Systemd service to enable WoL on startup
    ├── autosuspend.service       # Systemd service for executing idle checks
    └── autosuspend.timer         # Systemd timer to run idle sweeps every 5 minutes
```

---

## 🛠️ Step-by-Step Setup Guide

### 1. Disk & Storage Management (SSD + HDD Setup)

To ensure storage persistence, we mount the 512GB HDD at `/mnt/storage` using its unique UUID instead of volatile device paths like `/dev/sdb1`.

#### Step 1A: Identify the HDD UUID
Run the following command to find your partition UUID:
```bash
sudo blkid
```
Look for your 512GB partition (e.g., `/dev/sdb1`) and copy the `UUID="..."` value.

#### Step 1B: Create the Mount Points
```bash
sudo mkdir -p /mnt/storage
sudo mkdir -p /mnt/storage/media
sudo mkdir -p /mnt/storage/backups
```

#### Step 1C: Update `/etc/fstab`
Add the following line to the end of `/etc/fstab` (replace the UUID placeholder with your actual UUID):
```text
UUID=YOUR-HDD-UUID-HERE /mnt/storage ext4 defaults,nofail,noatime 0 2
```
> [!IMPORTANT]
> The `nofail` flag ensures that if the HDD is unplugged or fails to spin up, the server will still boot up using the SSD.

#### Step 1D: Mount the Drive & Set Permissions
```bash
sudo mount -a
sudo chown -R 1000:1000 /mnt/storage
sudo chmod -R 775 /mnt/storage
```

---

### 2. Services Configuration (Docker Stack)

The docker stack contains **Jellyfin** (configured for Intel QuickSync hardware transcoding), **Samba** (configured for backup shares), and **Tailscale** (for secure access).

#### Step 2A: Launch the Stack
From the root `telewol-media-server` directory, run:
```bash
docker compose up -d
```

#### Step 2B: Configure Samba Auth
To add credentials for user `home` within the Docker Samba container, run:
```bash
docker exec -it samba smbpasswd -a home
```
*(Enter a strong password when prompted)*

---

### 3. Wake-on-LAN (WoL) Implementation

WoL enables you to wake the server remotely by sending a Magic Packet, allowing you to turn it off when not in use.

#### Step 3A: BIOS Configuration
Reboot your server and enter the BIOS settings:
1. Enable **Wake-on-LAN**, **Power On By PCI-E**, or **PME Event Wake Up**.
2. **Disable** any deep sleep states (such as **ErP Ready** or **Deep Sleep Mode / C-States** optimization) which cut power to the network interface completely when the PC is off.

#### Step 3B: Deploy the Persistence Service
```bash
# 1. Copy the script to an executable location
sudo mkdir -p /opt/telewol/scripts
sudo cp scripts/enable-wol.sh /opt/telewol/scripts/
sudo chmod +x /opt/telewol/scripts/enable-wol.sh

# 2. Copy the systemd service
sudo cp systemd/wol.service /etc/systemd/system/

# 3. Enable and start the service
sudo systemctl daemon-reload
sudo systemctl enable --now wol.service
```

---

### 4. Automated Power Management (Auto-Suspend)

The auto-suspend daemon checks if the server is idle every 5 minutes and puts it to sleep if there is no activity.

#### Step 4A: Install the Scripts
```bash
# 1. Copy the suspend script
sudo cp scripts/autosuspend.sh /opt/telewol/scripts/
sudo chmod +x /opt/telewol/scripts/autosuspend.sh

# 2. Copy the systemd unit files
sudo cp systemd/autosuspend.service /etc/systemd/system/
sudo cp systemd/autosuspend.timer /etc/systemd/system/

# 3. Enable and start the timer
sudo systemctl daemon-reload
sudo systemctl enable --now autosuspend.timer
```

#### Step 4B: Monitoring Inactivity
To check when the next idle sweep is scheduled:
```bash
systemctl list-timers --all | grep autosuspend
```
To view log logs of the auto-suspend daemon:
```bash
journalctl -u autosuspend.service
```

---

### 5. Security & Network Hardening (UFW Firewall)

Secure the system by using UFW (Uncomplicated Firewall) to allow only necessary local and Tailscale traffic:

| Port | Service | Source Scope |
|---|---|---|
| `22/tcp` | SSH | Local subnet / Tailscale IP range |
| `139,445/tcp` | Samba (SMB) | Local subnet / Tailscale IP range |
| `8096/tcp` | Jellyfin Web | Local subnet / Tailscale IP range |
| `9/udp` | Wake-on-LAN | Anywhere (Broadcast packet) |

#### Setting Up Firewall Rules:
```bash
# Enable firewall default policies
sudo ufw default deny incoming
sudo ufw default allow outgoing

# Allow Wake-on-LAN magic packet port
sudo ufw allow 9/udp

# Allow local network/Tailscale connections to critical services
# (Replace 192.168.1.0/24 with your home subnet)
sudo ufw allow from 192.168.1.0/24 to any port 22 proto tcp
sudo ufw allow from 192.168.1.0/24 to any port 445 proto tcp
sudo ufw allow from 192.168.1.0/24 to any port 8096 proto tcp

# Enable firewall
sudo ufw enable
