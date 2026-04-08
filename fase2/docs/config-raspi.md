# 🍓 Raspberry Pi — Headless SSH + Docker

Guía para configurar una Raspberry Pi **sin monitor ni teclado**, conectada directamente a una PC por Ethernet, y correr contenedores Docker.

---

## Requisitos

- Raspberry Pi (3, 4 o 5)
- Tarjeta SD (8 GB mínimo, recomendado 16 GB+)
- Cable Ethernet (directo PC ↔ Pi, sin router)
- PC con Linux/Windows y lector de SD

---

## Paso 1 — Flashear la SD

Instalá [Raspberry Pi Imager](https://raspberrypi.com/software). En Debian:

```bash
wget https://downloads.raspberrypi.org/imager/imager_latest_amd64.deb
sudo apt install ./imager_latest_amd64.deb
sudo rpi-imager   # ejecutar con sudo para permisos de escritura
```

Seleccioná:
- **Dispositivo:** tu modelo de Pi
- **SO:** Raspberry Pi OS Lite (64-bit) — sin escritorio
- **Almacenamiento:** tu SD

---

## Paso 2 — Configurar antes de escribir

Antes de hacer clic en **Escribir**, abrí las opciones avanzadas (⚙) y configurá:

- ✅ SSH activado
- ✅ Usuario y contraseña (ej: `pi` / `tupassword`)
- ✅ Nombre del equipo (ej: `raspberrypi`)
- ❌ Sin WiFi — solo Ethernet

---

## Paso 3 — Conexión Ethernet directa

Sin router, hay que asignar IPs manuales en ambos extremos.

**En tu PC (Linux):**

```bash
sudo nmcli con mod 'Wired connection 1' \
  ipv4.method manual \
  ipv4.addresses 192.168.10.1/24
sudo nmcli con up 'Wired connection 1'
```

**En la SD, editá `/etc/dhcpcd.conf` antes de arrancar la Pi:**

```
interface eth0
static ip_address=192.168.10.2/24
static routers=192.168.10.1
static domain_name_servers=8.8.8.8
```

> Si no editás `dhcpcd.conf`, podés probar conectarte por mDNS: `ssh pi@raspberrypi.local`  
> (requiere `avahi-daemon` en Linux o Bonjour en Windows)

---

## Paso 4 — Conectarse por SSH

```bash
# Por IP estática
ssh pi@192.168.10.2

# O por mDNS
ssh pi@raspberrypi.local
```

La primera vez confirmar el fingerprint escribiendo `yes`.

**Desde Windows:** PowerShell (OpenSSH ya viene integrado en Win10/11) o [PuTTY](https://putty.org).

### Problemas comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `Connection refused` | SSH no activo | Revisá la config del Imager |
| `No route to host` | IP incorrecta o cable suelto | Verificar con `ip addr` |
| `Host key verification failed` | Cambió la clave del host | Borrar entrada en `~/.ssh/known_hosts` |

---

## Paso 5 — Instalar Docker

```bash
# Instalar Docker
curl -fsSL https://get.docker.com | sh

# Agregar usuario al grupo docker
sudo usermod -aG docker $USER

# Cerrar sesión SSH y volver a conectar, luego verificar:
docker run hello-world
```

### Docker Compose

```bash
sudo apt install docker-compose-plugin -y
docker compose version
```

---

## Paso 6 — Correr contenedores

Ejemplo con `docker-compose.yml`:

```yaml
services:
  web:
    image: nginx:alpine
    ports:
      - "8080:80"
    restart: unless-stopped
```

```bash
docker compose up -d      # levantar en background
docker ps                 # ver contenedores activos
docker compose logs -f    # ver logs en tiempo real
docker compose down       # detener
```

---

## Tips

**SSH sin contraseña (clave pública):**
```bash
ssh-copy-id pi@192.168.10.2
```

**Actualizar el sistema:**
```bash
sudo apt update && sudo apt upgrade -y
```

**Monitorear recursos:**
```bash
htop                      # CPU y memoria
df -h                     # espacio en disco
vcgencmd measure_temp     # temperatura de la CPU
```

---


*Raspberry Pi OS Lite (Bookworm) · Docker Engine · Abril 2026*
