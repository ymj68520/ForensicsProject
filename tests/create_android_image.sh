#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TESTS_DIR="$ROOT_DIR/tests"
IMAGE="$TESTS_DIR/android_test_gen.img"
SMS_DB="$TESTS_DIR/mmssms_test.db"
WA_DB="$TESTS_DIR/msgstore_test.db"
ASSETS_DIR="$TESTS_DIR/test_assets"

echo "Creating sample Android SQLite databases (SMS, WhatsApp)..."
command -v sqlite3 >/dev/null 2>&1 || { echo "sqlite3 not found. Please install sqlite3."; exit 1; }

rm -f "$SMS_DB" "$WA_DB"
sqlite3 "$SMS_DB" "CREATE TABLE sms (_id INTEGER PRIMARY KEY, address TEXT, body TEXT, date INTEGER, type INTEGER); INSERT INTO sms (_id, address, body, date, type) VALUES (1, '+1111111111', 'Test SMS message', 1609459200000, 1);"
sqlite3 "$WA_DB" "CREATE TABLE messages (_id INTEGER PRIMARY KEY, key_remote_jid TEXT, data TEXT, timestamp INTEGER); INSERT INTO messages (_id, key_remote_jid, data, timestamp) VALUES (1, '12345-67890@g.us', 'Hello from WhatsApp', 1609459201000);"

echo "Creating ext4 image file: $IMAGE (128MB)"
rm -f "$IMAGE"
fallocate -l 128M "$IMAGE"
command -v mkfs.ext4 >/dev/null 2>&1 || { echo "mkfs.ext4 not found. Please install e2fsprogs or mkfs tools."; exit 1; }
mkfs.ext4 -F -q "$IMAGE"

echo "Preparing additional test assets in: $ASSETS_DIR"
rm -rf "$ASSETS_DIR"
mkdir -p "$ASSETS_DIR"

# Create a variety of sample files to populate the image
echo "Sample text file" > "$ASSETS_DIR/sample.txt"
cat > "$ASSETS_DIR/sample.json" <<JSON
{"user":"alice","id":42,"active":true}
JSON
cat > "$ASSETS_DIR/sample.csv" <<CSV
id,name,score
1,Alice,95
2,Bob,88
CSV
echo "Hidden secret" > "$ASSETS_DIR/.hiddenfile"

# small PNG (1x1 transparent) base64
base64 -d > "$ASSETS_DIR/sample.png" <<'PNGBASE64'
iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGNgYAAAAAMAASsJTYQAAAAASUVORK5CYII=
PNGBASE64

# small JPEG (minimal) - use a tiny binary blob to represent an image
printf '%s' "\xFF\xD8\xFF\xD9" > "$ASSETS_DIR/sample.jpg"

# minimal PDF file (very small but valid header + trailer)
cat > "$ASSETS_DIR/sample.pdf" <<'PDF'
%PDF-1.1
1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
2 0 obj
<< /Type /Pages /Kids [3 0 R] /Count 1 >>
endobj
3 0 obj
<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R >>
endobj
4 0 obj
<< /Length 44 >>
stream
BT /F1 24 Tf 50 100 Td (Hello) Tj ET
endstream
endobj
xref
0 5
0000000000 65535 f 
0000000010 00000 n 
0000000060 00000 n 
0000000110 00000 n 
0000000200 00000 n 
trailer << /Root 1 0 R >>
startxref
300
%%EOF
PDF

# dummy apk file
dd if=/dev/urandom of="$ASSETS_DIR/sample.apk" bs=1 count=1024 2>/dev/null || true

# small executable script
cat > "$ASSETS_DIR/runme.sh" <<'SH'
#!/usr/bin/env bash
echo "Hello from test executable"
SH
chmod +x "$ASSETS_DIR/runme.sh"

# small log and config
echo "2025-01-01 00:00:00 App started" > "$ASSETS_DIR/app.log"
cat > "$ASSETS_DIR/config.ini" <<INI
[main]
theme=dark
version=1.2.3
INI

# create a modest binary file to simulate media / large file
dd if=/dev/urandom of="$ASSETS_DIR/large.bin" bs=1M count=4 2>/dev/null || head -c $((4*1024*1024)) < /dev/urandom > "$ASSETS_DIR/large.bin" 2>/dev/null || true

echo "Created $(ls -1 "$ASSETS_DIR" | wc -l) test assets"

echo "Writing example files and DBs into image using debugfs (if available), otherwise try mount fallback when running as root..."
if command -v debugfs >/dev/null 2>&1; then
    # Use debugfs to create directories and write DBs and assets
    debugfs -w "$IMAGE" <<EOF
mkdir /data
mkdir /data/data
mkdir /data/data/com.android.providers.telephony
mkdir /data/data/com.android.providers.telephony/databases
write "$SMS_DB" /data/data/com.android.providers.telephony/databases/mmssms.db
mkdir /data/data/com.whatsapp
mkdir /data/data/com.whatsapp/databases
write "$WA_DB" /data/data/com.whatsapp/databases/msgstore.db

# create typical sdcard and app paths
mkdir /sdcard
mkdir /sdcard/DCIM
mkdir /sdcard/DCIM/Camera
write "$ASSETS_DIR/sample.jpg" /sdcard/DCIM/Camera/sample.jpg
mkdir /sdcard/Download
write "$ASSETS_DIR/sample.pdf" /sdcard/Download/sample.pdf
write "$ASSETS_DIR/sample.png" /sdcard/Download/sample.png

mkdir /data/media
mkdir /data/media/0
mkdir /data/media/0/Download
write "$ASSETS_DIR/sample.apk" /data/media/0/Download/sample.apk

mkdir /data/data/com.example.app
mkdir /data/data/com.example.app/files
write "$ASSETS_DIR/sample.json" /data/data/com.example.app/files/data.json
write "$ASSETS_DIR/config.ini" /data/data/com.example.app/files/config.ini

mkdir /etc
write "$ASSETS_DIR/config.ini" /etc/config.ini

mkdir /var
mkdir /var/log
write "$ASSETS_DIR/app.log" /var/log/app.log

write "$ASSETS_DIR/.hiddenfile" /root/.hiddenfile

write "$ASSETS_DIR/large.bin" /mnt/sdcard/large.bin

# add executable and set its mode
write "$ASSETS_DIR/runme.sh" /data/local/tmp/runme.sh
chmod 0755 /data/local/tmp/runme.sh
quit
EOF
    echo "Files written to $IMAGE using debugfs"
else
    echo "debugfs not found."
    # If running as root try to mount and copy files (preserve permissions)
    if [ "$(id -u)" -eq 0 ] && command -v losetup >/dev/null 2>&1 && command -v mount >/dev/null 2>&1; then
        echo "Running as root: attaching loop device and mounting image to copy files..."
        LOOP=$(losetup -f --show "$IMAGE")
        mkdir -p /mnt/android_img
        mount "$LOOP" /mnt/android_img
        # create dirs and copy
        mkdir -p /mnt/android_img/data/data/com.android.providers.telephony/databases
        cp "$SMS_DB" /mnt/android_img/data/data/com.android.providers.telephony/databases/mmssms.db
        mkdir -p /mnt/android_img/data/data/com.whatsapp/databases
        cp "$WA_DB" /mnt/android_img/data/data/com.whatsapp/databases/msgstore.db

        mkdir -p /mnt/android_img/sdcard/DCIM/Camera
        cp "$ASSETS_DIR/sample.jpg" /mnt/android_img/sdcard/DCIM/Camera/sample.jpg
        mkdir -p /mnt/android_img/sdcard/Download
        cp "$ASSETS_DIR/sample.pdf" /mnt/android_img/sdcard/Download/sample.pdf

        mkdir -p /mnt/android_img/data/media/0/Download
        cp "$ASSETS_DIR/sample.apk" /mnt/android_img/data/media/0/Download/sample.apk

        mkdir -p /mnt/android_img/data/data/com.example.app/files
        cp "$ASSETS_DIR/sample.json" /mnt/android_img/data/data/com.example.app/files/data.json

        mkdir -p /mnt/android_img/var/log
        cp "$ASSETS_DIR/app.log" /mnt/android_img/var/log/app.log

        cp "$ASSETS_DIR/large.bin" /mnt/android_img/mnt/sdcard/large.bin
        cp "$ASSETS_DIR/runme.sh" /mnt/android_img/data/local/tmp/runme.sh
        chmod 0755 /mnt/android_img/data/local/tmp/runme.sh || true

        sync
        umount /mnt/android_img
        losetup -d "$LOOP"
        rmdir /mnt/android_img
        echo "Files copied into $IMAGE via loop mount"
    else
        echo "If you have root, the script can mount the image and copy assets for you. Example (requires sudo):"
        echo "  sudo losetup -f --show $IMAGE && sudo mount /dev/loopX /mnt && sudo cp -a $ASSETS_DIR/. /mnt/ && sudo umount /mnt && sudo losetup -d /dev/loopX"
        echo "Otherwise install debugfs (part of e2fsprogs) to write files without root."
    fi
fi

echo "Image creation complete: $IMAGE"
