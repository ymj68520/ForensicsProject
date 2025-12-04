Android Analyzer Tests
======================

本目录包含用于演示 Android 镜像分析的测试脚本与示例镜像。

文件说明
- `android_test.img`: 预先提供的 Android 测试镜像（只读）。
- `android_test_gen.img`: 脚本生成的示例 Android 镜像（如果使用脚本创建）。
- `create_android_image.sh`: 生成新的 ext4 镜像并在 `/data/data/.../databases` 路径写入示例 SQLite DB。它使用 `sqlite3`, `mkfs.ext4` 与 `debugfs`。
- `run_android_test.sh`: 运行基线测试（分析 `android_test.img`），再生成镜像并对 `android_test_gen.img` 运行分析；测试结果写入 `AndroidTestResults.md`。
- `AndroidTestResults.md`: 测试输出与结果摘要。

生成测试镜像的步骤
-------------------
1. 确保安装以下工具：`sqlite3`, `mkfs.ext4`（通常由 `e2fsprogs` 提供）, `debugfs`。如果缺少 `debugfs`，脚本会给出使用 `losetup` + `mount` 的替代建议（需要 `sudo`）。
2. 执行脚本：`./tests/create_android_image.sh`，它会在 `tests/` 内创建 `android_test_gen.img`。
3. 若系统支持，脚本将在镜像内写入 `mmssms.db` 和 `msgstore.db` 的示例内容，以供 `AndroidAnalyzer` 分析。

运行测试
--------
1. 确保构建产物 `build/forensic_analyzer` 可执行（项目根目录下运行 `cmake && make` 或等价构建命令）。
2. 运行 `./tests/run_android_test.sh` 来执行所有测试。测试输出将写入 `tests/AndroidTestResults.md`。

注意事项
--------
- 如果没有 `debugfs`，你可以手动把生成的数据库文件拷贝进镜像（需要 `sudo`）：
  sudo losetup -fP tests/android_test_gen.img
  sudo mount -o loop tests/android_test_gen.img /mnt
  sudo mkdir -p /mnt/data/data/com.android.providers.telephony/databases
  sudo cp tests/mmssms_test.db /mnt/data/data/com.android.providers.telephony/databases/mmssms.db
  sudo mkdir -p /mnt/data/data/com.whatsapp/databases
  sudo cp tests/msgstore_test.db /mnt/data/data/com.whatsapp/databases/msgstore.db
  sudo umount /mnt
  sudo losetup -d "/dev/loopX"

测试结果与输出
----------------
见 `AndroidTestResults.md`。

（本目录下的测试镜像和生成文件故意保留，不会自动清理。）
