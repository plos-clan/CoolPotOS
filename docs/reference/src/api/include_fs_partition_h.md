# include/fs/partition.h

## `#pragma once #define GPT_HEADER_SIGNATURE "EFI PART" #define MAX_PARTITIONS_NUM 128 #define MBR_MAX_PARTITION_NUM 4 #define PARTITION_TYPE_GPT 0xC12A7328 #define PARTITION_TYPE_MBR 0xEBD0A0A2 #define PARTITION_TYPE_UNKNOWN 0xFFFFFFFF #include "driver/blk_device.h" #include "types.h" struct GPT_DPT {`


注意: partition 分区初始化函数要在所有块设备驱动初始化之前
靠设备驱动主动去调用分区函数识别出分区


---

