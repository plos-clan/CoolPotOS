# driver/pci/msi.c

## `struct msi_msg_t *msi_arch_get_msg(struct msi_desc_t *msi_desc) {`


生成msi消息


- **`msi_desc`**: msi描述符
- **Returns**: struct msi_msg_t* msi消息指针（在描述符内）


---

## `static inline struct pci_msix_cap_t __msi_read_msix_cap_list(struct msi_desc_t *msi_desc, uint32_t cap_off) {`


读取msix的capability list


- **`msi_desc`**: msi描述符
- **`cap_off`**: capability list的offset
- **Returns**: struct pci_msix_cap_t 对应的capability list


---

## `static inline int __msix_map_table(pci_device_t *pci_dev, struct pci_msix_cap_t *msix_cap) {`


映射设备的msix表


- **`pci_dev`**: pci设备信息结构体
- **`msix_cap`**: msix capability list的结构体
- **Returns**: int 错误码


---

## `static inline void __msix_set_entry(struct msi_desc_t *msi_desc) {`


将msi_desc中的数据填写到msix表的指定表项处


- **`pci_dev`**: pci设备结构体
- **`msi_desc`**: msi描述符


---

## `static inline void __msix_clear_entry(pci_device_t *pci_dev, uint16_t msi_index) {`


清空设备的msix table的指定表项


- **`pci_dev`**: pci设备
- **`msi_index`**: 表项号


---

## `int pci_enable_msi(struct msi_desc_t *msi_desc) {`


启用 Message Signaled Interrupts


- **`header`**: 设备header
- **`vector`**: 中断向量号
- **`processor`**: 要投递到的处理器
- **`edge_trigger`**: 是否边缘触发
- **`assert`**: 是否高电平触发


- **Returns**: 返回码


---

