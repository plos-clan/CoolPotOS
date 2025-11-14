# include/driver/pci/msi.h

## `struct msi_msg_t {`


msi消息内容结构体



---

## `struct pci_msi_cap_t {`


msi capability list的结构



---

## `struct pci_msix_cap_t {`


MSI-X的capability list结构体



---

## `struct msi_desc_t {`


msi描述符



---

## `int pci_enable_msi(struct msi_desc_t *msi_desc);`


启用 Message Signaled Interrupts


- **`header`**: 设备header
- **`vector`**: 中断向量号
- **`processor`**: 要投递到的处理器
- **`edge_trigger`**: 是否边缘触发
- **`assert`**: 是否高电平触发


- **Returns**: 返回码


---

