# Android Analyzer Test Results

Generated: 2025-12-04T14:01:42+08:00

Summary
-------
- Baseline image: `tests/android_test.img` (provided)
  - Found: `contacts2.db`, `calllog.db`
  - Not found: `mmssms.db`, `msgstore.db`, `wa.db` (telephony and WhatsApp DBs)
  - Outcome: Successfully parsed Contacts (2 records) and CallLog (3 records)

- Generated image: `tests/android_test_gen.img` (created by `tests/create_android_image.sh`)
  - Created sample `mmssms.db` (SMS) and `msgstore.db` (WhatsApp) inside `/data/data/.../databases`
  - Outcome: Successfully parsed 1 SMS record and 1 WhatsApp message in the generated image
  - Not found in generated image: `contacts2.db`, `calllog.db` (these were not included in the generated image)

Steps to reproduce
------------------
1. Build the analyzer in the project root (CMake + make, or open the solution for Windows).
2. Generate the sample image and run the test runner script:

```
cd /home/ymj68520/ForensicsProject
chmod +x tests/create_android_image.sh tests/run_android_test.sh
./tests/run_android_test.sh
```

Notes on image creation
-----------------------
- `tests/create_android_image.sh` will:
  - Create sample SQLite DBs: `tests/mmssms_test.db` and `tests/msgstore_test.db`.
  - Create a 128MB ext4 image: `tests/android_test_gen.img`.
  - Use `debugfs` (if available) to create `/data/data/...` and write the DB files into the image.
- If `debugfs` isn't available, the script will show a `sudo losetup` + `mount` fallback that requires root to copy files manually into the image.

Artifacts
---------
- Input images (kept in tests/):
  - `tests/android_test.img` (baseline/example image)
  - `tests/android_test_gen.img` (generated image with mmssms & msgstore)
- Sample DBs created in tests/:
  - `tests/mmssms_test.db` (SMS sample)
  - `tests/msgstore_test.db` (WhatsApp sample)
- Generated database outputs from the analyzer (in repo root):
  - `android_test_raw.db`, `android_test_events.db`, `android_test_files.db` (from baseline)
  - `android_test_gen_raw.db`, `android_test_gen_events.db`, `android_test_gen_files.db` (from generated image)

Log (raw output from runs)
--------------------------
```
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
  Total events: 6
    - Creation events: 3
    - Modification events: 0
    - Access events: 2
    - Change events: 0
    - Deletion events: 1
Events extracted successfully
✓ Event database created: android_test_events.db

[3/3] Classifying files by type...
  File classification statistics:
    - Unknown Files: 1 files
    - Databases: 2 files
Files classified successfully
✓ File database created: android_test_files.db

[4/4] Analyzing Android application data...
Initializing file extractor...
  Image: /home/ymj68520/ForensicsProject/tests/android_test.img
  Database: android_test_raw.db
File extractor initialized successfully
Starting Android data analysis...
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
  Total events: 2
    - Creation events: 2
    - Modification events: 0
    - Access events: 0
    - Change events: 0
    - Deletion events: 0
Events extracted successfully
✓ Event database created: android_test_gen_events.db

[3/3] Classifying files by type...
  File classification statistics:
    - Databases: 2 files
Files classified successfully
✓ File database created: android_test_gen_files.db

[4/4] Analyzing Android application data...
Initializing file extractor...
  Image: /home/ymj68520/ForensicsProject/tests/android_test_gen.img
  Database: android_test_gen_raw.db
File extractor initialized successfully
Starting Android data analysis...
Parsed 1 records from SMS
SMS Record: address=+1111111111 body=Test SMS message date=1609459200000 id=1 type=1 
Error: File with path data/data/com.android.providers.contacts/databases/contacts2.db not found
Failed to extract database: data/data/com.android.providers.contacts/databases/contacts2.db
Error: File with path data/data/com.android.providers.contacts/databases/calllog.db not found
Failed to extract database: data/data/com.android.providers.contacts/databases/calllog.db
Parsed 1 records from WhatsApp
WhatsApp Record: data=Hello from WhatsApp id=1 remote_jid=12345-67890@g.us timestamp=1609459201000 
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
```
Running Android Analyzer tests
Generated on: 2025-12-04T14:01:42+08:00

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
  Total events: 6
    - Creation events: 3
    - Modification events: 0
    - Access events: 2
    - Change events: 0
    - Deletion events: 1
Events extracted successfully
✓ Event database created: android_test_events.db

[3/3] Classifying files by type...
  File classification statistics:
    - Unknown Files: 1 files
    - Databases: 2 files
Files classified successfully
✓ File database created: android_test_files.db

[4/4] Analyzing Android application data...
Initializing file extractor...
  Image: /home/ymj68520/ForensicsProject/tests/android_test.img
  Database: android_test_raw.db
File extractor initialized successfully
Starting Android data analysis...
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
Writing example DB files into image using debugfs...
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
  Total events: 2
    - Creation events: 2
    - Modification events: 0
    - Access events: 0
    - Change events: 0
    - Deletion events: 0
Events extracted successfully
✓ Event database created: android_test_gen_events.db

[3/3] Classifying files by type...
  File classification statistics:
    - Databases: 2 files
Files classified successfully
✓ File database created: android_test_gen_files.db

[4/4] Analyzing Android application data...
Initializing file extractor...
  Image: /home/ymj68520/ForensicsProject/tests/android_test_gen.img
  Database: android_test_gen_raw.db
File extractor initialized successfully
Starting Android data analysis...
Parsed 1 records from SMS
SMS Record: address=+1111111111 body=Test SMS message date=1609459200000 id=1 type=1 
Error: File with path data/data/com.android.providers.contacts/databases/contacts2.db not found
Failed to extract database: data/data/com.android.providers.contacts/databases/contacts2.db
Error: File with path data/data/com.android.providers.contacts/databases/calllog.db not found
Failed to extract database: data/data/com.android.providers.contacts/databases/calllog.db
Parsed 1 records from WhatsApp
WhatsApp Record: data=Hello from WhatsApp id=1 remote_jid=12345-67890@g.us timestamp=1609459201000 
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
