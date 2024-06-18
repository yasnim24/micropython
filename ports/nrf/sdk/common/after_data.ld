/*
 * This file is included by common.ld after the .data SECTION.
 */

/*
 * Originating from "config/nrf52840/armgcc/generic_gcc_nrf52.ld"
 * of "nRF5 SDK 15.3.0".
 */ 
SECTIONS
{
    . = ALIGN(4);
    .mem_section_dummy_ram :
    {
    }
    .cli_sorted_cmd_ptrs :
    {
        PROVIDE(__start_cli_sorted_cmd_ptrs = .);
        KEEP(*(.cli_sorted_cmd_ptrs))
        PROVIDE(__stop_cli_sorted_cmd_ptrs = .);
    } > RAM AT>FLASH_TEXT
    .fs_data :
    {
        PROVIDE(__start_fs_data = .);
        KEEP(*(.fs_data))
        PROVIDE(__stop_fs_data = .);
    } > RAM AT>FLASH_TEXT
    .log_dynamic_data :
    {
        PROVIDE(__start_log_dynamic_data = .);
        KEEP(*(SORT(.log_dynamic_data*)))
        PROVIDE(__stop_log_dynamic_data = .);
    } > RAM AT>FLASH_TEXT
    .log_filter_data :
    {
        PROVIDE(__start_log_filter_data = .);
        KEEP(*(SORT(.log_filter_data*)))
        PROVIDE(__stop_log_filter_data = .);
    } > RAM AT>FLASH_TEXT
}
