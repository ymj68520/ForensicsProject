Running Android Analyzer tests
Generated on: 2025-12-06T10:18:22+08:00

[TEST] Baseline run: existing image (/home/ymj68520/ForensicsProject/tests/android_test.img)
----------------------------------------------------------------
=== Forensic Image Analyzer ===
Image: /home/ymj68520/ForensicsProject/tests/android_test.img
Using The Sleuth Kit 4.14.0

[1/3] Analyzing image and extracting raw data...
Opening image: /home/ymj68520/ForensicsProject/tests/android_test.img
Image opened successfully
  Type: RAW/DD
  Size: 209715200 bytes (0.195312 GB)
No volume system detected, trying direct filesystem access...
Filesystem opened successfully
  Type: ext4
  Block size: 4096
  Block count: 51200
  Root inode: 2
Walking filesystem (root inode: 2)...
  Processing: .
  Processing: ..
  Processing: lost+found
  Processing: lost+found/.
  Processing: lost+found/..
  Processing: data
  Processing: data/.
  Processing: data/..
  Processing: data/data
  Processing: data/data/.
Filesystem walk completed
✓ Raw database created: android_test_raw.db

[2/3] Extracting filesystem events...
  Total events: 60
    - Creation events: 48
    - Modification events: 0
    - Access events: 12
    - Change events: 0
    - Deletion events: 0
Events extracted successfully
✓ Event database created: android_test_events.db

[3/3] Classifying files by type...
  File classification statistics:
    - Executables: 6 files
    - Unknown Files: 12 files
    - Archives: 18 files
    - Databases: 12 files
Files classified successfully
✓ File database created: android_test_files.db

[4/4] Analyzing Android application data...
Initializing file extractor...
  Image: /home/ymj68520/ForensicsProject/tests/android_test.img
  Database: android_test_raw.db
File extractor initialized successfully
Starting Android data analysis...
Starting system directory analysis...
Build Properties:
ro.build.id=TP1A.220624.014
ro.build.display.id=TP1A.220624.014
ro.build.version.incremental=8650216
ro.build.version.sdk=33
ro.build.version.preview_sdk=0
ro.build.version.codename=REL
ro.build.version.all_codenames=REL
ro.build.version.release=13
ro.build.version.release_or_codename=13
ro.build.version.security_patch=2022-08-05
ro.build.version.base_os=
ro.build.version.min_supported_target_sdk=23
ro.build.date=Wed Aug 10 00:00:00 UTC 2022
ro.build.date.utc=1660089600
ro.build.type=user
ro.build.user=android-build
ro.build.host=abfarm000
ro.build.tags=release-keys
ro.build.flavor=generic_x86_64-user
ro.product.model=Android SDK built for x86_64
ro.product.brand=generic
ro.product.name=generic_x86_64
ro.product.device=generic_x86_64
ro.product.board=
ro.product.cpu.abi=x86_64
ro.product.cpu.abilist=x86_64,x86
ro.product.cpu.abilist32=x86
ro.product.cpu.abilist64=x86_64
ro.product.manufacturer=Google
ro.product.locale=en-US
ro.product.first_api_level=23
ro.board.platform=generic_x86_64
ro.build.product=generic_x86_64
ro.build.description=generic_x86_64-user 13 TP1A.220624.014 8650216 release-keys
ro.build.fingerprint=generic/generic_x86_64/generic_x86_64:13/TP1A.220624.014/8650216:user/release-keys
ro.build.characteristics=default
System App: SystemUI at system/app/SystemUI.apk
System App: Settings at system/priv-app/Settings.apk
System App: SystemUI at system/app/SystemUI.apk
System App: Settings at system/priv-app/Settings.apk
Framework files:
framework.jar
framework.dex
framework.so
System directory analysis completed.
Error: File with path data/data/com.android.providers.telephony/databases/mmssms.db not found
Failed to extract database: data/data/com.android.providers.telephony/databases/mmssms.db
Parsed 2 records from Contacts
Contacts Record: display_name=John Doe id=1 phone=+1234567890 
Contacts Record: display_name=Jane Smith id=2 phone=+0987654321 
Parsed 3 records from CallLog
CallLog Record: date=1640995200000 duration=120 id=1 number=+1234567890 type=1 
CallLog Record: date=1641081600000 duration=60 id=2 number=+0987654321 type=2 
CallLog Record: date=1641168000000 duration=300 id=3 number=+1122334455 type=1 
Error: File with path data/data/com.whatsapp/databases/msgstore.db not found
Failed to extract database: data/data/com.whatsapp/databases/msgstore.db
Error: File with path data/data/com.whatsapp/databases/wa.db not found
Failed to extract database: data/data/com.whatsapp/databases/wa.db
Android data analysis completed.
✓ Android data analysis completed

=== Analysis Complete ===
Generated databases:
  1. android_test_raw.db (Raw TSK data)
  2. android_test_events.db (Filesystem events)
  3. android_test_files.db (Classified files)

Tip: Use --database to extract files from the image

[TEST] Generate new Android test image with sample DBs (/home/ymj68520/ForensicsProject/tests/android_test_gen.img)
----------------------------------------------------------------
Creating sample Android SQLite databases (SMS, WhatsApp)...
Creating ext4 image file: /home/ymj68520/ForensicsProject/tests/android_test_gen.img (128MB)
Preparing additional test assets in: /home/ymj68520/ForensicsProject/tests/test_assets
Created 11 test assets
Creating system directory assets...
System assets created
Writing example files and DBs into image using debugfs (if available), otherwise try mount fallback when running as root...
debugfs 1.46.5 (30-Dec-2021)
debugfs:  mkdir /data
debugfs:  mkdir /data/data
debugfs:  mkdir /data/data/com.android.providers.telephony
debugfs:  mkdir /data/data/com.android.providers.telephony/databases
debugfs:  write "/home/ymj68520/ForensicsProject/tests/mmssms_test.db" /data/data/com.android.providers.telephony/databases/mmssms.db
Allocated inode: 16
debugfs:  mkdir /data/data/com.whatsapp
debugfs:  mkdir /data/data/com.whatsapp/databases
debugfs:  write "/home/ymj68520/ForensicsProject/tests/msgstore_test.db" /data/data/com.whatsapp/databases/msgstore.db
Allocated inode: 19
debugfs:  
debugfs:  # create typical sdcard and app paths
debugfs: Unknown request "#".  Type "?" for a request list.
debugfs:  mkdir /sdcard
debugfs:  mkdir /sdcard/DCIM
debugfs:  mkdir /sdcard/DCIM/Camera
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/sample.jpg" /sdcard/DCIM/Camera/sample.jpg
Allocated inode: 23
debugfs:  mkdir /sdcard/Download
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/sample.pdf" /sdcard/Download/sample.pdf
Allocated inode: 25
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/sample.png" /sdcard/Download/sample.png
Allocated inode: 26
debugfs:  
debugfs:  # create system directory
debugfs: Unknown request "#".  Type "?" for a request list.
debugfs:  mkdir /system
debugfs:  mkdir /system/app
debugfs:  mkdir /system/priv-app
debugfs:  mkdir /system/framework
debugfs:  write "/home/ymj68520/ForensicsProject/tests/system_assets/build.prop" /system/build.prop
Allocated inode: 31
debugfs:  write "/home/ymj68520/ForensicsProject/tests/system_assets/SystemUI.apk" /system/app/SystemUI.apk
Allocated inode: 32
debugfs:  write "/home/ymj68520/ForensicsProject/tests/system_assets/Settings.apk" /system/priv-app/Settings.apk
Allocated inode: 33
debugfs:  write "/home/ymj68520/ForensicsProject/tests/system_assets/framework.jar" /system/framework/framework.jar
Allocated inode: 34
debugfs:  write "/home/ymj68520/ForensicsProject/tests/system_assets/framework.dex" /system/framework/framework.dex
Allocated inode: 35
debugfs:  write "/home/ymj68520/ForensicsProject/tests/system_assets/framework.so" /system/framework/framework.so
Allocated inode: 36
debugfs:  
debugfs:  mkdir /data/media
debugfs:  mkdir /data/media/0
debugfs:  mkdir /data/media/0/Download
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/sample.apk" /data/media/0/Download/sample.apk
Allocated inode: 40
debugfs:  
debugfs:  mkdir /data/data/com.example.app
debugfs:  mkdir /data/data/com.example.app/files
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/sample.json" /data/data/com.example.app/files/data.json
Allocated inode: 43
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/config.ini" /data/data/com.example.app/files/config.ini
Allocated inode: 44
debugfs:  
debugfs:  mkdir /etc
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/config.ini" /etc/config.ini
Allocated inode: 46
debugfs:  
debugfs:  mkdir /var
debugfs:  mkdir /var/log
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/app.log" /var/log/app.log
Allocated inode: 49
debugfs:  
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/.hiddenfile" /root/.hiddenfile
/root: File not found by ext2_lookup while looking up "/root"
write: File not found by ext2_lookup 
debugfs:  
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/large.bin" /mnt/sdcard/large.bin
/mnt/sdcard: File not found by ext2_lookup while looking up "/mnt/sdcard"
write: File not found by ext2_lookup 
debugfs:  
debugfs:  # add executable and set its mode
debugfs: Unknown request "#".  Type "?" for a request list.
debugfs:  write "/home/ymj68520/ForensicsProject/tests/test_assets/runme.sh" /data/local/tmp/runme.sh
/data/local/tmp: File not found by ext2_lookup while looking up "/data/local/tmp"
write: File not found by ext2_lookup 
debugfs:  chmod 0755 /data/local/tmp/runme.sh
debugfs: Unknown request "chmod".  Type "?" for a request list.
debugfs:  quit
Files written to /home/ymj68520/ForensicsProject/tests/android_test_gen.img using debugfs
Image creation complete: /home/ymj68520/ForensicsProject/tests/android_test_gen.img

[TEST] Run analyzer on generated image (/home/ymj68520/ForensicsProject/tests/android_test_gen.img)
----------------------------------------------------------------
=== Forensic Image Analyzer ===
Image: /home/ymj68520/ForensicsProject/tests/android_test_gen.img
Using The Sleuth Kit 4.14.0

[1/3] Analyzing image and extracting raw data...
Opening image: /home/ymj68520/ForensicsProject/tests/android_test_gen.img
Image opened successfully
  Type: RAW/DD
  Size: 134217728 bytes (0.125 GB)
No volume system detected, trying direct filesystem access...
Filesystem opened successfully
  Type: ext4
  Block size: 4096
  Block count: 32768
  Root inode: 2
Walking filesystem (root inode: 2)...
  Processing: .
  Processing: ..
  Processing: lost+found
  Processing: lost+found/.
  Processing: lost+found/..
  Processing: data
  Processing: data/.
  Processing: data/..
  Processing: data/data
  Processing: data/data/.
Filesystem walk completed
✓ Raw database created: android_test_gen_raw.db

[2/3] Extracting filesystem events...
  Total events: 78
    - Creation events: 78
    - Modification events: 0
    - Access events: 0
    - Change events: 0
    - Deletion events: 0
Events extracted successfully
✓ Event database created: android_test_gen_events.db

[3/3] Classifying files by type...
  File classification statistics:
    - Executables: 3 files
    - Unknown Files: 6 files
    - Documents: 12 files
    - Images: 12 files
    - Archives: 15 files
    - System Files: 12 files
    - Web Files: 6 files
    - Databases: 12 files
Files classified successfully
✓ File database created: android_test_gen_files.db

[4/4] Analyzing Android application data...
Initializing file extractor...
  Image: /home/ymj68520/ForensicsProject/tests/android_test_gen.img
  Database: android_test_gen_raw.db
File extractor initialized successfully
Starting Android data analysis...
Starting system directory analysis...
Build Properties:
ro.build.id=TP1A.220624.014
ro.build.display.id=TP1A.220624.014
ro.build.version.incremental=8650216
ro.build.version.sdk=33
ro.build.version.preview_sdk=0
ro.build.version.codename=REL
ro.build.version.all_codenames=REL
ro.build.version.release=13
ro.build.version.release_or_codename=13
ro.build.version.security_patch=2022-08-05
ro.build.version.base_os=
ro.build.version.min_supported_target_sdk=23
ro.build.date=Wed Aug 10 00:00:00 UTC 2022
ro.build.date.utc=1660089600
ro.build.type=user
ro.build.user=android-build
ro.build.host=abfarm000
ro.build.tags=release-keys
ro.build.flavor=generic_x86_64-user
ro.product.model=Android SDK built for x86_64
ro.product.brand=generic
ro.product.name=generic_x86_64
ro.product.device=generic_x86_64
ro.product.board=
ro.product.cpu.abi=x86_64
ro.product.cpu.abilist=x86_64,x86
ro.product.cpu.abilist32=x86
ro.product.cpu.abilist64=x86_64
ro.product.manufacturer=Google
ro.product.locale=en-US
ro.product.first_api_level=23
ro.board.platform=generic_x86_64
ro.build.product=generic_x86_64
ro.build.description=generic_x86_64-user 13 TP1A.220624.014 8650216 release-keys
ro.build.fingerprint=generic/generic_x86_64/generic_x86_64:13/TP1A.220624.014/8650216:user/release-keys
ro.build.characteristics=default
System App: SystemUI at system/app/SystemUI.apk
System App: Settings at system/priv-app/Settings.apk
System App: SystemUI at system/app/SystemUI.apk
System App: Settings at system/priv-app/Settings.apk
Framework files:
framework.jar
framework.dex
framework.so
System directory analysis completed.
Parsed 1 records from SMS
SMS Record: address=+1111111111 body=Test SMS message date=1609459200000 id=1 type=1 
Error: File with path data/data/com.android.providers.contacts/databases/contacts2.db not found
Failed to extract database: data/data/com.android.providers.contacts/databases/contacts2.db
Error: File with path data/data/com.android.providers.contacts/databases/calllog.db not found
Failed to extract database: data/data/com.android.providers.contacts/databases/calllog.db
Error: File with path data/data/com.whatsapp/databases/wa.db not found
Failed to extract database: data/data/com.whatsapp/databases/wa.db
Android data analysis completed.
✓ Android data analysis completed

=== Analysis Complete ===
Generated databases:
  1. android_test_gen_raw.db (Raw TSK data)
  2. android_test_gen_events.db (Filesystem events)
  3. android_test_gen_files.db (Classified files)

Tip: Use --database to extract files from the image

Test run complete. Please inspect /home/ymj68520/ForensicsProject/tests/AndroidTestResults.md for details.
Note: The generated image /home/ymj68520/ForensicsProject/tests/android_test_gen.img and the created database files are intentionally not cleaned up as requested.

### Database Storage Test Results

#### Generated Database Files
- `android_test.img_system_analysis.db` - System analysis results for android_test.img
- `android_test_gen.img_system_analysis.db` - System analysis results for android_test_gen.img

#### Database Schema
```sql
-- Build properties table
CREATE TABLE system_build_properties (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    property_key TEXT NOT NULL,
    property_value TEXT,
    UNIQUE(property_key)
);

-- System apps table  
CREATE TABLE system_apps (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    package_name TEXT NOT NULL,
    apk_path TEXT NOT NULL,
    version_name TEXT,
    version_code TEXT,
    is_system_app INTEGER DEFAULT 1,
    is_privileged INTEGER DEFAULT 0,
    UNIQUE(package_name, apk_path)
);

-- Framework files table
CREATE TABLE framework_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_name TEXT NOT NULL,
    file_path TEXT NOT NULL,
    file_type TEXT,
    file_size INTEGER,
    UNIQUE(file_path)
);
```

#### Sample Data Verification

**Build Properties (36 records):**
```
ro.build.id|TP1A.220624.014
ro.build.display.id|TP1A.220624.014
ro.build.version.incremental|8650216
ro.build.version.sdk|33
...
```

**System Apps (2 records):**
```
1|SystemUI|system/app/SystemUI.apk|Unknown|Unknown|1|0
2|Settings|system/priv-app/Settings.apk|Unknown|Unknown|1|1
```

**Framework Files (3 records):**
```
1|framework.jar|system/framework/framework.jar|jar|29
2|framework.dex|system/framework/framework.dex|dex|29  
3|framework.so|system/framework/framework.so|so|28
```

### Analysis
✅ **Database Creation**: Successfully creates separate system analysis database
✅ **Data Persistence**: All analysis results properly stored in SQLite database
✅ **Data Integrity**: Unique constraints prevent duplicate entries
✅ **Query Capability**: Database can be queried for forensic analysis
✅ **Separation of Concerns**: System analysis data separated from raw filesystem data
