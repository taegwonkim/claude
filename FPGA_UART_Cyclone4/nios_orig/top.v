// Copyright (C) 2021  Intel Corporation. All rights reserved.
// Your use of Intel Corporation's design tools, logic functions 
// and other software and tools, and any partner logic 
// functions, and any output files from any of the foregoing 
// (including device programming or simulation files), and any 
// associated documentation or information are expressly subject 
// to the terms and conditions of the Intel Program License 
// Subscription Agreement, the Intel Quartus Prime License Agreement,
// the Intel FPGA IP License Agreement, or other applicable license
// agreement, including, without limitation, that your use is for
// the sole purpose of programming logic devices manufactured by
// Intel and sold by Intel or its authorized distributors.  Please
// refer to the applicable agreement for further details, at
// https://fpgasoftware.intel.com/eula.

module top
(
// {ALTERA_ARGS_BEGIN} DO NOT REMOVE THIS LINE!

	ADC01_CLK,
	ADC01_DATA,
	ADC01_NOE,
	ADC23_CLK,
	ADC23_DATA,
	ADC23_NOE,
	ADC45_CLK,
	ADC45_DATA,
	ADC45_NOE,
	CLK_50M,
	LED_ERROR,
	LED_FPGA,
	LED_NIOS,
	LED_WIFI,
	MCU_RX,
	MCU_TX,
	RESET_N,
	SPI1_MISO,
	SPI1_MOSI,
	SPI1_NSS,
	SPI1_SCK,
	SPI1_NHOLD,
	SPI1_NWP,
	TP_IN,
	TP_OUT,
	UART1_RX,
	UART1_TX,
	UART2_DE,
	UART2_RX,
	UART2_TX
// {ALTERA_ARGS_END} DO NOT REMOVE THIS LINE!
	

);


	// {ALTERA_IO_BEGIN} DO NOT REMOVE THIS LINE!
	input		wire		CLK_50M;
	input		wire		RESET_N;

	output	 		LED_ERROR;
	output			LED_FPGA;
	output			LED_NIOS;
	output			LED_WIFI;

	output	wire		MCU_RX;
	input		wire		MCU_TX;

	input		wire		UART1_RX;
	output	wire		UART1_TX;
	output	wire		UART2_DE;

	input		wire		UART2_RX;
	output	wire		UART2_TX;

	output						ADC01_CLK;
	input		wire	[11:0]	ADC01_DATA;
	output						ADC01_NOE;
	
	output						ADC23_CLK;
	input		wire	[11:0]	ADC23_DATA;
	output						ADC23_NOE;
	
	output						ADC45_CLK;
	input		wire	[11:0]	ADC45_DATA;
	output						ADC45_NOE;

	input		wire		SPI1_MISO;
	output	wire		SPI1_MOSI;
	output	wire		SPI1_NSS;
	output	wire		SPI1_SCK;
	output				SPI1_NHOLD;
	output				SPI1_NWP;

	input		wire	[3:0]		TP_IN;
	output	wire	[3:0]		TP_OUT;

	// {ALTERA_IO_END} DO NOT REMOVE THIS LINE!
	
	// LED -----------------------------------------------------------------------------------
	wire  [3:0] led;
	assign LED_NIOS   = led[0];
	//assign LED_FPGA   = led[1];
	assign LED_WIFI   = MCU_TX;//led[2];
	assign LED_ERROR  = led[3];

	// SPI Flash Control
	wire	[1:0] spi1_cont;
	assign SPI1_NHOLD  = spi1_cont[0];
	assign SPI1_NWP    = spi1_cont[1];

	// Test Point ----------------------------------------------------------------------------
	//wire tp_out[3:0];
	//wire tp_in[3:0];
	
	//assign TP1_OUT  = tp_out[0];
	//assign TP2_OUT  = tp_out[1];
	//assign TP3_OUT  = tp_out[2];
	//assign TP4_OUT  = tp_out[3];
	

	// ADC -----------------------------------------------------------------------------------
	// ADC Control
	reg	adc_noe = 1'b1;
	assign ADC01_NOE  = adc_noe;
	assign ADC23_NOE  = adc_noe;
	assign ADC45_NOE  = adc_noe;

	// ADC Clock
	wire	adc_clk01;
	wire	adc_clk23;
	wire	adc_clk45;
	
	assign ADC01_CLK   = adc_clk01;
	assign ADC23_CLK   = adc_clk23;
	assign ADC45_CLK   = adc_clk45;

	// ADC Data

	reg [7:0] adc0_addr_cnt;
	reg [7:0] adc1_addr_cnt;
	reg [7:0] adc2_addr_cnt;
	reg [7:0] adc3_addr_cnt;
	reg [7:0] adc4_addr_cnt;
	reg [7:0] adc5_addr_cnt;

	reg [31:0] adc0_mem [31:0];
	reg [31:0] adc1_mem [31:0];
	reg [31:0] adc2_mem [31:0];
	reg [31:0] adc3_mem [31:0];
	reg [31:0] adc4_mem [31:0];
	reg [31:0] adc5_mem [31:0];
	
	reg [11:0] adc0_data;
	reg [11:0] adc1_data;
	reg [11:0] adc2_data;
	reg [11:0] adc3_data;
	reg [11:0] adc4_data;
	reg [11:0] adc5_data;
	
	// ADC Dual Port RAM Clock ---------------------------------------------------------------
	wire dpram_clk01;
	wire dpram_clk23;
	wire dpram_clk45;

	// ADC0 Dual Port RAM --------------------------------------------------------------------
	wire dpram_adc0_chips;
	wire dpram_adc0_clken;
	wire dpram_adc0_write;

	reg dpram_adc0_chips_reg = 1'b0;
	reg dpram_adc0_clken_reg = 1'b0;
	reg dpram_adc0_write_reg = 1'b0;
	
	assign dpram_adc0_chips = dpram_adc0_chips_reg;
	assign dpram_adc0_clken = dpram_adc0_clken_reg;
	assign dpram_adc0_write = dpram_adc0_write_reg;
	
	// ADC1 Dual Port RAM --------------------------------------------------------------------
	wire dpram_adc1_chips;
	wire dpram_adc1_clken;
	wire dpram_adc1_write;

	reg dpram_adc1_chips_reg = 1'b0;
	reg dpram_adc1_clken_reg = 1'b0;
	reg dpram_adc1_write_reg = 1'b0;
	
	assign dpram_adc1_chips = dpram_adc1_chips_reg;
	assign dpram_adc1_clken = dpram_adc1_clken_reg;
	assign dpram_adc1_write = dpram_adc1_write_reg;
	
	// ADC2 Dual Port RAM --------------------------------------------------------------------
	wire dpram_adc2_chips;
	wire dpram_adc2_clken;
	wire dpram_adc2_write;

	reg dpram_adc2_chips_reg = 1'b0;
	reg dpram_adc2_clken_reg = 1'b0;
	reg dpram_adc2_write_reg = 1'b0;
	
	assign dpram_adc2_chips = dpram_adc2_chips_reg;
	assign dpram_adc2_clken = dpram_adc2_clken_reg;
	assign dpram_adc2_write = dpram_adc2_write_reg;
	
	// ADC3 Dual Port RAM --------------------------------------------------------------------
	wire dpram_adc3_chips;
	wire dpram_adc3_clken;
	wire dpram_adc3_write;

	reg dpram_adc3_chips_reg = 1'b0;
	reg dpram_adc3_clken_reg = 1'b0;
	reg dpram_adc3_write_reg = 1'b0;
	
	assign dpram_adc3_chips = dpram_adc3_chips_reg;
	assign dpram_adc3_clken = dpram_adc3_clken_reg;
	assign dpram_adc3_write = dpram_adc3_write_reg;
	
	// ADC4 Dual Port RAM --------------------------------------------------------------------
	wire dpram_adc4_chips;
	wire dpram_adc4_clken;
	wire dpram_adc4_write;

	reg dpram_adc4_chips_reg = 1'b0;
	reg dpram_adc4_clken_reg = 1'b0;
	reg dpram_adc4_write_reg = 1'b0;
	
	assign dpram_adc4_chips = dpram_adc4_chips_reg;
	assign dpram_adc4_clken = dpram_adc4_clken_reg;
	assign dpram_adc4_write = dpram_adc4_write_reg;
	
	// ADC5 Dual Port RAM --------------------------------------------------------------------
	wire dpram_adc5_chips;
	wire dpram_adc5_clken;
	wire dpram_adc5_write;

	reg dpram_adc5_chips_reg = 1'b0;
	reg dpram_adc5_clken_reg = 1'b0;
	reg dpram_adc5_write_reg = 1'b0;
	
	assign dpram_adc5_chips = dpram_adc5_chips_reg;
	assign dpram_adc5_clken = dpram_adc5_clken_reg;
	assign dpram_adc5_write = dpram_adc5_write_reg;

	//************************************************************************************************
	// Main Loop Clock ---------------------------------------------------------------
	wire main_clk;
	assign  main_clk = CLK_50M;

	// NIOS II from (Rx) / to (Tx) -----------------------------------------------------------
	wire [5:0]  nios_rx;
	wire [5:0]  nios_tx;
	wire [11:0] nios_tx_comp;
	
	reg  [5:0]  nios_rx_reg;
	
	assign nios_rx = nios_rx_reg;
	
	// MCU Handshake Signal --------------------------------------------
	//wire		mcu_rx;
	//wire		mcu_tx;
	
	//assign	MCU_RX =	mcu_rx;

	// Over Voltage --------------------------------------------
	reg  [5:0]  adc_ov_flag;

	//-- LED_FPGA -------------------------------------------------------------------------
	reg [31:0]  main_1ms_cnt;
	reg [31:0]  led_fpga_cnt;

	reg led_fpga_bit;
	assign	LED_FPGA = led_fpga_bit;

// {ALTERA_MODULE_BEGIN} DO NOT REMOVE THIS LINE!
	
	//************************************************************************************************
	//----------------------------------------------------------------------------------------
	initial 
		begin
			adc_noe = 1'b0;
			
			dpram_adc0_chips_reg = 1'b1;
			dpram_adc0_clken_reg = 1'b1;
			dpram_adc0_write_reg = 1'b1;

			dpram_adc1_chips_reg = 1'b1;
			dpram_adc1_clken_reg = 1'b1;
			dpram_adc1_write_reg = 1'b1;

			dpram_adc2_chips_reg = 1'b1;
			dpram_adc2_clken_reg = 1'b1;
			dpram_adc2_write_reg = 1'b1;

			dpram_adc3_chips_reg = 1'b1;
			dpram_adc3_clken_reg = 1'b1;
			dpram_adc3_write_reg = 1'b1;

			dpram_adc4_chips_reg = 1'b1;
			dpram_adc4_clken_reg = 1'b1;
			dpram_adc4_write_reg = 1'b1;

			dpram_adc5_chips_reg = 1'b1;
			dpram_adc5_clken_reg = 1'b1;
			dpram_adc5_write_reg = 1'b1;

			nios_rx_reg = 6'b0;

			adc_ov_flag[5:0] = 5'b0;
			
		end
		
	//----------------------------------------------------------------------------------------

	//-- Main ----------------------------------------------------------------	
	always @(posedge main_clk)
		begin
			led_fpga_cnt <= led_fpga_cnt + 1;
			
			if(led_fpga_cnt > 40000000)
				begin
					led_fpga_cnt <= 0;
					led_fpga_bit <= led_fpga_bit ^ 1'b1;
				end
		
		end

	//*************************************************************************************
	//-- ADC0 ----------------------------------------------------------------	
	always @(negedge adc_clk01)
		begin
			adc0_data[11:0] = ADC01_DATA[11:0];
			
			if(nios_tx[0] == 1'b1)
				begin
					if(nios_rx_reg[0] == 1'b0)
						begin
							nios_rx_reg[0]   = 1'b1;
						end
				end
			else
				begin
					if(nios_rx_reg[0] == 1'b1)
						begin
							nios_rx_reg[0] = 1'b0;
							adc_ov_flag[0] = 1'b0;
							
							adc0_mem[1] = 0;
							adc0_mem[2] = 0;
							adc0_mem[3] = 0;
						end
					else
						begin
							adc0_mem[0] = adc0_data[11:0];
							
							if(adc0_mem[0] > 2047)
								begin
									adc0_mem[0] = 0;
								end
				
							adc0_mem[2] <= adc0_mem[2] + 1;		// adc normal count

							if(adc0_mem[0][11:0] > nios_tx_comp[11:0])
								begin
									if(adc_ov_flag[0] == 1'b1)
										begin
											adc0_mem[3] <= adc0_mem[3] + 1;		// adc over count
											
											if(adc0_mem[1] < adc0_mem[0]) 
												begin
													adc0_mem[1] = adc0_mem[0];	// ADC max data
												end
										end
									else
										begin
											adc_ov_flag[0] = 1'b1;
										end
								end
							else
								begin
									adc_ov_flag[0] = 1'b0;
									
									if(adc0_mem[1] < adc0_mem[0]) 
										begin
											adc0_mem[1] = adc0_mem[0];
										end
								end
						end
				end
		end

		
	
	//-- ADC1 -------------------------------------------------------------------	
	always @(posedge adc_clk01)
		begin
			adc1_data[11:0] = ADC01_DATA[11:0];
			
			if(nios_tx[1] == 1'b1)
				begin
					if(nios_rx_reg[1] == 1'b0)
						begin
							nios_rx_reg[1]   = 1'b1;
						end
					end
			else
				begin
					if(nios_rx_reg[1] == 1'b1)
						begin
							nios_rx_reg[1] = 1'b0;
							adc_ov_flag[1] = 1'b0;
							
							adc1_mem[1] = 0;
							adc1_mem[2] = 0;
							adc1_mem[3] = 0;
						end
					else
						begin
							adc1_mem[0] = adc1_data[11:0];
						
							if(adc1_mem[0] > 2047)
								begin
									adc1_mem[0] = 0;
								end
							
							adc1_mem[2] <= adc1_mem[2] + 1;		// adc normal count

							if(adc1_mem[0][11:0] > nios_tx_comp[11:0])
								begin
									if(adc_ov_flag[1] == 1'b1)
										begin
											adc1_mem[3] <= adc1_mem[3] + 1;		// adc over count
											
											if(adc1_mem[1] < adc1_mem[0]) 
												begin
													adc1_mem[1] = adc1_mem[0];	// ADC max data
												end
										end
									else
										begin
											adc_ov_flag[1] = 1'b1;
										end
								end
							else
								begin
									adc_ov_flag[1] = 1'b0;
									
									if(adc1_mem[1] < adc1_mem[0]) 
										begin
											adc1_mem[1] = adc1_mem[0];
										end
								end

						end
				end
	
		end
	
	//-- ADC2 -------------------------------------------------------------------	
	always @(negedge adc_clk23)
		begin
			if(nios_tx[2] == 1'b1)
				begin
					if(nios_rx_reg[2] == 1'b0)
						begin
							nios_rx_reg[2]   = 1'b1;
						end
					end
			else
				begin
					if(nios_rx_reg[2] == 1'b1)
						begin
							nios_rx_reg[2] = 1'b0;
							adc_ov_flag[2] = 1'b0;
							
							adc2_mem[1] = 0;
							adc2_mem[2] = 0;
							adc2_mem[3] = 0;
						end
					else
						begin
							adc2_mem[0] = ADC23_DATA[11:0];
						
							if(adc2_mem[0] > 2047)
								begin
									adc2_mem[0] = 0;
								end
							
							adc2_mem[2] <= adc2_mem[2] + 1;		// adc normal count

							if(adc2_mem[0][11:0] > nios_tx_comp[11:0])
								begin
									if(adc_ov_flag[2] == 1'b1)
										begin
											adc2_mem[3] <= adc2_mem[3] + 1;		// adc over count
											
											if(adc2_mem[1] < adc2_mem[0]) 
												begin
													adc2_mem[1] = adc2_mem[0];	// ADC max data
												end
										end
									else
										begin
											adc_ov_flag[2] = 1'b1;
										end
								end
							else
								begin
									adc_ov_flag[2] = 1'b0;
									
									if(adc2_mem[1] < adc2_mem[0]) 
										begin
											adc2_mem[1] = adc2_mem[0];
										end
								end

						end
				end
	
		end
	
	//-- ADC3 ----------------------------------------------------------------------
	always @(posedge adc_clk23)
		begin
			if(nios_tx[3] == 1'b1)
				begin
					if(nios_rx_reg[3] == 1'b0)
						begin
							nios_rx_reg[3]   = 1'b1;
						end
				end
			else
				begin
					if(nios_rx_reg[3] == 1'b1)
						begin
							nios_rx_reg[3] = 1'b0;
							adc_ov_flag[3] = 1'b0;
							
							adc3_mem[1] = 0;
							adc3_mem[2] = 0;
							adc3_mem[3] = 0;
						end
					else
						begin
							adc3_mem[0] = ADC23_DATA[11:0];
							
							if(adc3_mem[0] > 2047)
								begin
									adc3_mem[0] = 0;
								end
							
							adc3_mem[2] <= adc3_mem[2] + 1;		// adc normal count

							if(adc3_mem[0][11:0] > nios_tx_comp[11:0])
								begin
									if(adc_ov_flag[3] == 1'b1)
										begin
											adc3_mem[3] <= adc3_mem[3] + 1;		// adc over count
											
											if(adc3_mem[1] < adc3_mem[0]) 
												begin
													adc3_mem[1] = adc3_mem[0];	// ADC max data
												end
										end
									else
										begin
											adc_ov_flag[3] = 1'b1;
										end
								end
							else
								begin
									adc_ov_flag[3] = 1'b0;
									
									if(adc3_mem[1] < adc3_mem[0]) 
										begin
											adc3_mem[1] = adc3_mem[0];
										end
								end

						end
				end
	
		end
	
	//-- ADC4 -------------------------------------------------------------------	
	always @(negedge adc_clk45)
		begin
			if(nios_tx[4] == 1'b1)
				begin
					if(nios_rx_reg[4] == 1'b0)
						begin
							nios_rx_reg[4]   = 1'b1;
						end
					end
			else
				begin
					if(nios_rx_reg[4] == 1'b1)
						begin
							nios_rx_reg[4] = 1'b0;
							adc_ov_flag[4] = 1'b0;
							
							adc4_mem[1] = 0;
							adc4_mem[2] = 0;
							adc4_mem[3] = 0;
						end
					else
						begin
							adc4_mem[0] = ADC45_DATA[11:0];
						
							if(adc4_mem[0] > 2047)
								begin
									adc4_mem[0] = 0;
								end
							
							adc4_mem[2] <= adc4_mem[2] + 1;		// adc normal count

							if(adc4_mem[0][11:0] > nios_tx_comp[11:0])
								begin
									if(adc_ov_flag[4] == 1'b1)
										begin
											adc4_mem[3] <= adc4_mem[3] + 1;		// adc over count
											
											if(adc4_mem[1] < adc4_mem[0]) 
												begin
													adc4_mem[1] = adc4_mem[0];	// ADC max data
												end
										end
									else
										begin
											adc_ov_flag[4] = 1'b1;
										end
								end
							else
								begin
									adc_ov_flag[4] = 1'b0;
									
									if(adc4_mem[1] < adc4_mem[0]) 
										begin
											adc4_mem[1] = adc4_mem[0];
										end
								end

						end
				end
	
		end
	
	//-- ADC5 ----------------------------------------------------------------------
	always @(posedge adc_clk45)
		begin
			if(nios_tx[5] == 1'b1)
				begin
					if(nios_rx_reg[5] == 1'b0)
						begin
							nios_rx_reg[5]   = 1'b1;
						end
				end
			else
				begin
					if(nios_rx_reg[5] == 1'b1)
						begin
							nios_rx_reg[5] = 1'b0;
							adc_ov_flag[5] = 1'b0;
							
							adc5_mem[1] = 0;
							adc5_mem[2] = 0;
							adc5_mem[3] = 0;
						end
					else
						begin
							adc5_mem[0] = ADC45_DATA[11:0];
							
							if(adc5_mem[0] > 2047)
								begin
									adc5_mem[0] = 0;
								end
							
							adc5_mem[2] <= adc5_mem[2] + 1;		// adc normal count

							if(adc5_mem[0][11:0] > nios_tx_comp[11:0])
								begin
									if(adc_ov_flag[5] == 1'b1)
										begin
											adc5_mem[3] <= adc5_mem[3] + 1;		// adc over count
											
											if(adc5_mem[1] < adc5_mem[0]) 
												begin
													adc5_mem[1] = adc5_mem[0];	// ADC max data
												end
										end
									else
										begin
											adc_ov_flag[5] = 1'b1;
										end
								end
							else
								begin
									adc_ov_flag[5] = 1'b0;
									
									if(adc5_mem[1] < adc5_mem[0]) 
										begin
											adc5_mem[1] = adc5_mem[0];
										end
								end

						end
				end
	
		end

	//*************************************************************************************
	//-- ADC0,1 DPRAM Address Increment ----------------------------------------------- 
	always @(posedge dpram_clk01)
		begin
			adc0_addr_cnt = adc0_addr_cnt + 1;
			
			if(adc0_addr_cnt > 3)
				begin
					adc0_addr_cnt = 0;
				end
				
			adc1_addr_cnt = adc1_addr_cnt + 1;
			
			if(adc1_addr_cnt > 3)
				begin
					adc1_addr_cnt = 0;
				end
		end

	//-- ADC2,3 DPRAM Address Increment ------------------------------------------------ 
	always @(posedge dpram_clk23)
		begin
			adc2_addr_cnt = adc2_addr_cnt + 1;
			
			if(adc2_addr_cnt > 3)
				begin
					adc2_addr_cnt = 0;
				end
			adc3_addr_cnt = adc3_addr_cnt + 1;
			
			if(adc3_addr_cnt > 3)
				begin
					adc3_addr_cnt = 0;
				end
		end

	//-- ADC4,5 DPRAM Address Increment ------------------------------------------------ 
	always @(posedge dpram_clk45)
		begin
			adc4_addr_cnt = adc4_addr_cnt + 1;
			
			if(adc4_addr_cnt > 3)
				begin
					adc4_addr_cnt = 0;
				end
			adc5_addr_cnt = adc5_addr_cnt + 1;
			
			if(adc5_addr_cnt > 3)
				begin
					adc5_addr_cnt = 0;
				end
		end

	
	// {ALTERA_MODULE_END} DO NOT REMOVE THIS LINE!
	//**************************************************************************************************************************************
	sgdt_dc6ch_wifi u0 (
	
		//-- PLL0
		.altpll_0_areset_conduit_export            (),            //            altpll_0_areset_conduit.export
		.altpll_0_c1_clk                           (adc_clk01),                           //                        altpll_0_c1.clk
		.altpll_0_c2_clk                           (adc_clk23),                           //                        altpll_0_c2.clk
		.altpll_0_c3_clk                           (adc_clk45),                            //                        altpll_0_c3.clk
		.altpll_0_locked_conduit_export            (),            //            altpll_0_locked_conduit.export
		//-- PLL1
		.altpll_1_areset_conduit_export            (),            //            altpll_0_areset_conduit.export
		//.altpll_1_c0_clk                           (),                           //                        altpll_1_c1.clk
		.altpll_1_c1_clk                           (dpram_clk01),                           //                        altpll_1_c1.clk
		.altpll_1_c2_clk                           (dpram_clk23),                           //                        altpll_1_c2.clk
		.altpll_1_c3_clk                           (dpram_clk45),                            //                        altpll_1_c3.clk
		.altpll_1_locked_conduit_export            (),            //            altpll_0_locked_conduit.export
		.clk_clk                                   (CLK_50M),                                   //                                clk.clk
		//-- ADC0
		.dpram_adc0_clk2_clk                       (dpram_clk01),                       //                    dpram_adc5_clk2.clk
		.dpram_adc0_reset2_reset                   (~RESET_N),                   //                  dpram_adc5_reset2.reset
		.dpram_adc0_reset2_reset_req               (),               //                                   .reset_req
		.dpram_adc0_s2_address                     (adc0_addr_cnt),                     //                      dpram_adc5_s2.address
		.dpram_adc0_s2_chipselect                  (dpram_adc0_chips),                  //                                   .chipselect
		.dpram_adc0_s2_clken                       (dpram_adc0_clken),                       //                                   .clken
		.dpram_adc0_s2_write                       (dpram_adc0_write),                       //                                   .write
		.dpram_adc0_s2_readdata                    (),                    //                                   .readdata
		.dpram_adc0_s2_writedata                   (adc0_mem[adc0_addr_cnt]),                   //                                   .writedata
		.dpram_adc0_s2_byteenable                  (4'b1111),                  //                                   .byteenable
		//-- ADC1
		.dpram_adc1_clk2_clk                       (dpram_clk01),                       //                    dpram_adc5_clk2.clk
		.dpram_adc1_reset2_reset                   (~RESET_N),                   //                  dpram_adc5_reset2.reset
		.dpram_adc1_reset2_reset_req               (),               //                                   .reset_req
		.dpram_adc1_s2_address                     (adc1_addr_cnt),                     //                      dpram_adc5_s2.address
		.dpram_adc1_s2_chipselect                  (dpram_adc1_chips),                  //                                   .chipselect
		.dpram_adc1_s2_clken                       (dpram_adc1_clken),                       //                                   .clken
		.dpram_adc1_s2_write                       (dpram_adc1_write),                       //                                   .write
		.dpram_adc1_s2_readdata                    (),                    //                                   .readdata
		.dpram_adc1_s2_writedata                   (adc1_mem[adc1_addr_cnt]),                   //                                   .writedata
		.dpram_adc1_s2_byteenable                  (4'b1111),                  //                                   .byteenable
		//-- ADC2
		.dpram_adc2_clk2_clk                       (dpram_clk23),                       //                    dpram_adc5_clk2.clk
		.dpram_adc2_reset2_reset                   (~RESET_N),                   //                  dpram_adc5_reset2.reset
		.dpram_adc2_reset2_reset_req               (),               //                                   .reset_req
		.dpram_adc2_s2_address                     (adc2_addr_cnt),                     //                      dpram_adc5_s2.address
		.dpram_adc2_s2_chipselect                  (dpram_adc2_chips),                  //                                   .chipselect
		.dpram_adc2_s2_clken                       (dpram_adc2_clken),                       //                                   .clken
		.dpram_adc2_s2_write                       (dpram_adc2_write),                       //                                   .write
		.dpram_adc2_s2_readdata                    (),                    //                                   .readdata
		.dpram_adc2_s2_writedata                   (adc2_mem[adc2_addr_cnt]),                   //                                   .writedata
		.dpram_adc2_s2_byteenable                  (4'b1111),                  //                                   .byteenable
		//-- ADC3
		.dpram_adc3_clk2_clk                       (dpram_clk23),                       //                    dpram_adc5_clk2.clk
		.dpram_adc3_reset2_reset                   (~RESET_N),                   //                  dpram_adc5_reset2.reset
		.dpram_adc3_reset2_reset_req               (),               //                                   .reset_req
		.dpram_adc3_s2_address                     (adc3_addr_cnt),                     //                      dpram_adc5_s2.address
		.dpram_adc3_s2_chipselect                  (dpram_adc3_chips),                  //                                   .chipselect
		.dpram_adc3_s2_clken                       (dpram_adc3_clken),                       //                                   .clken
		.dpram_adc3_s2_write                       (dpram_adc3_write),                       //                                   .write
		.dpram_adc3_s2_readdata                    (),                    //                                   .readdata
		.dpram_adc3_s2_writedata                   (adc3_mem[adc3_addr_cnt]),                   //                                   .writedata
		.dpram_adc3_s2_byteenable                  (4'b1111),                  //                                   .byteenable
		//-- ADC4
		.dpram_adc4_clk2_clk                       (dpram_clk45),                       //                    dpram_adc5_clk2.clk
		.dpram_adc4_reset2_reset                   (~RESET_N),                   //                  dpram_adc5_reset2.reset
		.dpram_adc4_reset2_reset_req               (),               //                                   .reset_req
		.dpram_adc4_s2_address                     (adc4_addr_cnt),                     //                      dpram_adc5_s2.address
		.dpram_adc4_s2_chipselect                  (dpram_adc4_chips),                  //                                   .chipselect
		.dpram_adc4_s2_clken                       (dpram_adc4_clken),                       //                                   .clken
		.dpram_adc4_s2_write                       (dpram_adc4_write),                       //                                   .write
		.dpram_adc4_s2_readdata                    (),                    //                                   .readdata
		.dpram_adc4_s2_writedata                   (adc4_mem[adc4_addr_cnt]),                   //                                   .writedata
		.dpram_adc4_s2_byteenable                  (4'b1111),                  //                                   .byteenable
		//-- ADC5
		.dpram_adc5_clk2_clk                       (dpram_clk45),                       //                    dpram_adc5_clk2.clk
		.dpram_adc5_reset2_reset                   (~RESET_N),                   //                  dpram_adc5_reset2.reset
		.dpram_adc5_reset2_reset_req               (),               //                                   .reset_req
		.dpram_adc5_s2_address                     (adc5_addr_cnt),                     //                      dpram_adc5_s2.address
		.dpram_adc5_s2_chipselect                  (dpram_adc5_chips),                  //                                   .chipselect
		.dpram_adc5_s2_clken                       (dpram_adc5_clken),                       //                                   .clken
		.dpram_adc5_s2_write                       (dpram_adc5_write),                       //                                   .write
		.dpram_adc5_s2_readdata                    (),                    //                                   .readdata
		.dpram_adc5_s2_writedata                   (adc5_mem[adc5_addr_cnt]),                   //                                   .writedata
		.dpram_adc5_s2_byteenable                  (4'b1111),                  //                                   .byteenable
		//-- RESET_N
		.reset_reset_n                             (RESET_N),                             //                              reset.reset_n
		//-- SPI_FLASH
		.spi_flash_external_MISO                   (SPI1_MISO),                   //                 spi_flash_external.MISO
		.spi_flash_external_MOSI                   (SPI1_MOSI),                   //                                   .MOSI
		.spi_flash_external_SCLK                   (SPI1_SCK),                   //                                   .SCLK
		.spi_flash_external_SS_n                   (SPI1_NSS),                   //                                   .SS_n
		.spi_flash_cont_external_connection_export (spi1_cont), // spi_flash_cont_external_connection.export
		//-- UART1
		.uart1_external_connection_rxd             (UART1_RX),             //          uart1_external_connection.rxd
		.uart1_external_connection_txd             (UART1_TX),             //                                   .txd
		//-- UART2
		.uart2_external_connection_rxd             (UART2_RX),             //          uart2_external_connection.rxd
		.uart2_external_connection_txd             (UART2_TX),             //                                   .txd
		.uart2_de_external_connection_export       (UART2_DE),       //       uart2_de_external_connection.export
		//-- LED
		.led_out_external_connection_export        (led),        //        led_out_external_connection.export
		//-- NIOS RX/TX
		.nios_rx_external_connection_export        (nios_rx),        //        nios_rx_external_connection.export
		.nios_tx_external_connection_export        (nios_tx),        //        nios_tx_external_connection.export
		//-- NIOS COMP_VALUE
		.nios_tx_comp_external_connection_export   (nios_tx_comp),   //   nios_tx_comp_external_connection.export
		//-- MCU TX/RX
		.mcu_rx_external_connection_export         (MCU_RX),         //         mcu_rx_external_connection.export
		.mcu_tx_external_connection_export         (MCU_TX),         //         mcu_tx_external_connection.export
		//-- TEST POINT
		.tp_in_external_connection_export     		 (TP_IN),     //     test_point_external_connection.export
		.tp_out_external_connection_export         (TP_OUT)          //         tp_out_external_connection.export
	);

endmodule

