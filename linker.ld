ENTRY(_start)
 
SECTIONS
{
	
	. = 2M;
 
	.text BLOCK(4K) : ALIGN(4K)
	{
	    code = .; _code = .; __code = .;
		*(.multiboot)
		*(.text)
	}
 
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}
 
	.data BLOCK(4K) : ALIGN(4K)
	{
	    data = .; _data = .; __data = .;
		*(.data)
	}
 
	.bss BLOCK(4K) : ALIGN(4K)
	{
	    bss = .; _bss = .; __bss = .;
		*(COMMON)
		*(.bss)
	}
 
	end = .; _end = .; __end = .;
}