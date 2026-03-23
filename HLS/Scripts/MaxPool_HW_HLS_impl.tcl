open_project MaxPool_HW_HLS
set_top MaxPool_HW
add_files HLS/HLS_MaxPool/maxpool.h
add_files HLS/HLS_MaxPool/maxpool.cpp
open_solution "solution1" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default
config_export -display_name MaxPool_HW -format ip_catalog -output ./IP-repo/MaxPool_HW.zip -rtl vhdl -vendor EPFL -vivado_clock 10
config_interface -m_axi_addr64=0
csynth_design
export_design -rtl vhdl -format ip_catalog -output ./IP-repo/MaxPool_HW.zip
quit
