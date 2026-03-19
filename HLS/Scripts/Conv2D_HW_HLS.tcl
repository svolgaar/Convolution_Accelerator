open_project Conv2D_HW_HLS
set_top Conv2D_HW
add_files HLS/conv2d.h
add_files HLS/conv2d.cpp
add_files -tb HLS/conv2DTestbench.cpp
open_solution "solution1" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default
config_export -display_name Conv2D_HW -format ip_catalog -output ./IP-repo/Conv2D_HW.zip -rtl vhdl -vendor EPFL -vivado_clock 10
config_interface -m_axi_addr64=0
quit
