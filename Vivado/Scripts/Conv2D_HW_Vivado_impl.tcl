# Usage: vivado -mode batch -source run_vivado.tcl

open_project Conv2D_HW_Vivado/Conv2D_HW_Vivado.xpr
update_compile_order -fileset sources_1

# Refresh the IP catalog
update_ip_catalog
# Upgrade all IP instances in the project
upgrade_ip [get_ips]
# Report the status of IP instances
report_ip_status

reset_run [get_runs synth_1]
reset_run [get_runs impl_1]

launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

write_hw_platform -fixed -include_bit -force -file ./Conv2D_HW_Vivado/design_1_wrapper.xsa

# Check if implementation was successful
set status [get_property STATUS [get_runs impl_1]]
if {$status ne "write_bitstream Complete!"} {
    puts $status
    puts "Error: Implementation failed"
    exit 1
}

#open_run impl_1
#report_power -file design_power_report.txt
#report_timing -file design_timing_report.txt
#report_utilization -file design_utilization_report.txt

close_project
exit
