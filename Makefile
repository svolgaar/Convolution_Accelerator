.PHONY: ip ip_conv ip_maxpool hls_project hls_sim clean cleanall vivado_project bitstream extract_bitstream help
CONV_PROJECT := Conv2D_HW
MAXPOOL_PROJECT := MaxPool_HW

help:
	@echo ""
	@echo "MAKEFILE targets"
	@echo ""
	@echo "VITIS HLS targets"
	@echo ""
	@echo "hls_project: Just creates the Vitis HLS project (Conv2D)"
	@echo "hls_sim: Creates the Vitis HLS project and runs the C++ simulation (Conv2D)"
	@echo "ip: Synthesizes and exports both Conv2D and MaxPool IP cores"
	@echo "ip_conv: Synthesizes and exports the Conv2D IP core only"
	@echo "ip_maxpool: Synthesizes and exports the MaxPool IP core only"
	@echo ""
	@echo "VIVADO targets"
	@echo ""
	@echo "vivado_project: Just creates the Vivado project"
	@echo "bitstream: Creates the Vivado project and runs synthesis up to bitstream generation"
	@echo "extract_bitstream: If the Vivado bitstream has already been generated, extracts it to this folder"
	@echo ""
	@echo "Generic targets"
	@echo ""
	@echo "clean: Deletes log files and Vitis HLS and Vivado projects. Deletes the files in the IP Catalog (keeps the IP catalog ZIP files)"
	@echo "cleanall: Additionally, deletes the bitstream files and the IP catalog ZIP files"
	@echo "help: Shows this help message"
	@echo ""


CONV_HLS_SRC := HLS/HLS_Conv
MAXPOOL_HLS_SRC := HLS/HLS_MaxPool
HLS_SCRIPTS := HLS/Scripts
Vivado_SCRIPTS := Vivado/Scripts

ip: ip_conv ip_maxpool

ip_conv: IP-repo/$(CONV_PROJECT).zip

IP-repo/$(CONV_PROJECT).zip: $(CONV_HLS_SRC)/conv2d.cpp $(CONV_HLS_SRC)/conv2d.h
	rm -rf $(CONV_PROJECT)_HLS
	vitis_hls -f $(HLS_SCRIPTS)/$(CONV_PROJECT)_HLS_impl.tcl
	@echo ""
	@echo "========================================"
	@echo "== Conv2D HLS Synthesis Report"
	@echo "========================================"
	@cat $(CONV_PROJECT)_HLS/solution1/syn/report/csynth.rpt

ip_maxpool: IP-repo/$(MAXPOOL_PROJECT).zip

IP-repo/$(MAXPOOL_PROJECT).zip: $(MAXPOOL_HLS_SRC)/maxpool.cpp $(MAXPOOL_HLS_SRC)/maxpool.h
	rm -rf $(MAXPOOL_PROJECT)_HLS
	vitis_hls -f $(HLS_SCRIPTS)/$(MAXPOOL_PROJECT)_HLS_impl.tcl
	@echo ""
	@echo "========================================"
	@echo "== MaxPool HLS Synthesis Report"
	@echo "========================================"
	@cat $(MAXPOOL_PROJECT)_HLS/solution1/syn/report/csynth.rpt

hls_project: $(CONV_PROJECT)_HLS/hls.app

$(CONV_PROJECT)_HLS/hls.app:
	rm -rf $(CONV_PROJECT)_HLS
	vitis_hls -f $(HLS_SCRIPTS)/$(CONV_PROJECT)_HLS.tcl

hls_sim:
	rm -rf $(CONV_PROJECT)_HLS
	vitis_hls -f $(HLS_SCRIPTS)/$(CONV_PROJECT)_HLS_sim.tcl



vivado_project: ip $(CONV_PROJECT)_Vivado/

$(CONV_PROJECT)_Vivado/:
	mkdir -p IP-repo/$(CONV_PROJECT) IP-repo/$(MAXPOOL_PROJECT)
	cd IP-repo/$(CONV_PROJECT); unzip -o ../$(CONV_PROJECT).zip
	cd IP-repo/$(MAXPOOL_PROJECT); unzip -o ../$(MAXPOOL_PROJECT).zip
	vivado -mode batch -source $(Vivado_SCRIPTS)/$(CONV_PROJECT)_Vivado.tcl -tclargs --project_name $(CONV_PROJECT)_Vivado

bitstream: vivado_project $(CONV_PROJECT).bit $(CONV_PROJECT).hwh

$(CONV_PROJECT).bit $(CONV_PROJECT).hwh:
	vivado -mode batch -source $(Vivado_SCRIPTS)/$(CONV_PROJECT)_Vivado_impl.tcl -tclargs --project_name $(CONV_PROJECT)_Vivado
	cp ./$(CONV_PROJECT)_Vivado/$(CONV_PROJECT)_Vivado.runs/impl_1/design_1_wrapper.bit $(CONV_PROJECT).bit
	cp ./$(CONV_PROJECT)_Vivado/$(CONV_PROJECT)_Vivado.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh $(CONV_PROJECT).hwh
	cp ./$(CONV_PROJECT)_Vivado/$(CONV_PROJECT)_Vivado.runs/impl_1/design_1_wrapper.ltx $(CONV_PROJECT).ltx


extract_bitstream:
	cp ./$(CONV_PROJECT)_Vivado/$(CONV_PROJECT)_Vivado.runs/impl_1/design_1_wrapper.bit $(CONV_PROJECT).bit
	cp ./$(CONV_PROJECT)_Vivado/$(CONV_PROJECT)_Vivado.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh $(CONV_PROJECT).hwh
	cp ./$(CONV_PROJECT)_Vivado/$(CONV_PROJECT)_Vivado.runs/impl_1/design_1_wrapper.ltx $(CONV_PROJECT).ltx


clean:
	rm -rf NA/ .Xil
	rm -f vivado*.jou vivado*.log vivado*.str vitis_hls.log
	rm -f $(CONV_PROJECT)_Vivado_def_val.txt $(CONV_PROJECT)_Vivado_dump.txt
	rm -rf $(CONV_PROJECT)_HLS $(CONV_PROJECT)_Vivado
	rm -rf $(MAXPOOL_PROJECT)_HLS
	rm -rf IP-repo/$(CONV_PROJECT)/ IP-repo/$(MAXPOOL_PROJECT)/
	rm -f IP-repo/component.xml
	rm -rf IP-repo/constraints/ IP-repo/doc/ IP-repo/drivers/ IP-repo/hdl/ IP-repo/misc/ IP-repo/xgui/

cleanall: clean
	rm -f IP-repo/$(CONV_PROJECT).zip IP-repo/$(MAXPOOL_PROJECT).zip
	rm -f *.bit *.hwh *.ltx

