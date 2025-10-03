ENTRY(_start)

SECTIONS
{

	. = 2M;

	.multiboot :
	{ 
		*(.multiboot)
	}

	.text :
	{
		*(.text)
	}

	.rodata ALIGN(0x1000):
	{
		*(.rodata)
	}

	.data ALIGN(0x1000):
	{
		*(.data)
	}

	.bss :
	{
		*(COMMON)
		*(.bss)
	}
}