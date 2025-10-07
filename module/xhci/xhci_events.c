#include "lock.h"
#include "proc_subsystem.h"
#include "xhci_hcd.h"

spin_t xhci_event_handle_lock = SPIN_INIT;

// 事件处理函数
void xhci_handle_events(xhci_hcd_t *xhci) {
    spin_lock(xhci_event_handle_lock);

    xhci_ring_t *event_ring       = xhci->event_ring;
    int          events_processed = 0;

    while (events_processed < 256) { // 避免无限循环
        xhci_trb_t *trb = &event_ring->trbs[event_ring->dequeue_index];

        // 检查cycle bit
        uint8_t cycle = (trb->control & TRB_FLAG_CYCLE) ? 1 : 0;
        if (cycle != event_ring->cycle_state) {
            break; // 没有更多事件
        }

        uint32_t trb_type = (trb->control >> 10) & 0x3F;

        switch (trb_type) {
        case TRB_TYPE_TRANSFER: xhci_complete_transfer(xhci, trb); break;

        case TRB_TYPE_CMD_COMPLETE: xhci_complete_command(xhci, trb); break;

        case TRB_TYPE_PORT_STATUS: {
            uint32_t port_id = ((trb->parameter >> 24) & 0xFF) - 1;
            printk("XHCI: Port status change on port %d\n", port_id);
            // xhci_handle_port_status(xhci, port_id);
        } break;

        default: printk("XHCI: Unknown event type %d\n", trb_type); break;
        }

        // 移动到下一个TRB
        event_ring->dequeue_index++;
        if (event_ring->dequeue_index >= event_ring->size) {
            event_ring->dequeue_index = 0;
            event_ring->cycle_state   = !event_ring->cycle_state;
        }

        events_processed++;

        // 更新ERDP (Event Ring Dequeue Pointer)
        uint64_t erdp = event_ring->phys_addr + (event_ring->dequeue_index * sizeof(xhci_trb_t));
        xhci_writeq(&xhci->intr_regs[0].erdp, erdp | (1 << 3)); // Set EHB
    }

    spin_unlock(xhci_event_handle_lock);
}

void xhci_event_handler(xhci_hcd_t *xhci) {
    while (xhci->event_thread.running) {
        xhci_handle_events(xhci);

        scheduler_yield();
    }

    task_exit(0);
}

// 启动事件处理线程
int xhci_start_event_handler(xhci_hcd_t *xhci) {
    xhci->event_thread.running = true;
    xhci->event_thread.xhci    = xhci;

    task_create("xhci_event_handler", (void (*)(uint64_t))xhci_event_handler, (uint64_t)xhci,
                KTHREAD_PRIORITY);

    printk("XHCI: Event handler thread created\n");
    return 0;
}

// 停止事件处理线程
void xhci_stop_event_handler(xhci_hcd_t *xhci) {
    if (xhci->event_thread.running) {
        xhci->event_thread.running = false;
        printk("XHCI: Event handler thread joined\n");
    }
}
