Renesas RZ/Five SoC Platform
============================
The RZ/Five microprocessor includes a RISC-V CPU Core (AX45MP Single)
1.0 GHz, 16-bit DDR3L/DDR4 interface. And it also has many interfaces
such as Gbit-Ether, CAN, and USB 2.0, making it ideal for applications
such as entry-class social infrastructure gateway control and industrial
gateway control.

To build platform specific library and firmwares, provide the
*PLATFORM=renesas/rzfive* parameter to the top level make command.

Platform Options
----------------

The Renesas RZ/Five platform does not have any platform-specific options.

Building Renesas RZ/Five Platform
-----------------------------

```
make PLATFORM=renesas/rzfive
```

DTS Example: (RZ/Five AX45MP)
-------------------------------

```
	cpus {
		#address-cells = <1>;
		#size-cells = <0>;
		timebase-frequency = <12000000>;

		cpu0: cpu@0 {
			compatible = "andestech,ax45mp", "riscv";
			device_type = "cpu";
			reg = <0x0>;
			status = "okay";
			riscv,isa = "rv64imafdc";
			mmu-type = "riscv,sv39";
			i-cache-size = <0x8000>;
			i-cache-line-size = <0x40>;
			d-cache-size = <0x8000>;
			d-cache-line-size = <0x40>;
			clocks = <&cpg CPG_CORE R9A07G043_AX45MP_CORE0_CLK>,
				 <&cpg CPG_CORE R9A07G043_AX45MP_ACLK>;

			cpu0_intc: interrupt-controller {
				#interrupt-cells = <1>;
				compatible = "riscv,cpu-intc";
				interrupt-controller;
			};
		};
	};

	soc {
		compatible = "simple-bus";
		#address-cells = <1>;
		#size-cells = <0>;
		ranges;

		scif0: serial@1004b800 {
			compatible = "renesas,scif-r9a07g043",
				     "renesas,scif-r9a07g044";
			reg = <0 0x1004b800 0 0x400>;
			interrupts = <412 IRQ_TYPE_LEVEL_HIGH>,
				     <414 IRQ_TYPE_LEVEL_HIGH>,
				     <415 IRQ_TYPE_LEVEL_HIGH>,
				     <413 IRQ_TYPE_LEVEL_HIGH>,
				     <416 IRQ_TYPE_LEVEL_HIGH>,
				     <416 IRQ_TYPE_LEVEL_HIGH>;
			interrupt-names = "eri", "rxi", "txi",
					  "bri", "dri", "tei";
			clocks = <&cpg CPG_MOD R9A07G043_SCIF0_CLK_PCK>;
			clock-names = "fck";
			power-domains = <&cpg>;
			resets = <&cpg R9A07G043_SCIF0_RST_SYSTEM_N>;
			status = "disabled";
		};

		cpg: clock-controller@11010000 {
			compatible = "renesas,r9a07g043-cpg";
			reg = <0 0x11010000 0 0x10000>;
			clocks = <&extal_clk>;
			clock-names = "extal";
			#clock-cells = <2>;
			#reset-cells = <1>;
			#power-domain-cells = <0>;
		};

		sysc: system-controller@11020000 {
			compatible = "renesas,r9a07g043-sysc";
			reg = <0 0x11020000 0 0x10000>;
			status = "disabled";
		};

		pinctrl: pinctrl@11030000 {
			compatible = "renesas,r9a07g043-pinctrl";
			reg = <0 0x11030000 0 0x10000>;
			gpio-controller;
			#gpio-cells = <2>;
			#interrupt-cells = <2>;
			interrupt-controller;
			gpio-ranges = <&pinctrl 0 0 152>;
			clocks = <&cpg CPG_MOD R9A07G043_GPIO_HCLK>;
			power-domains = <&cpg>;
			resets = <&cpg R9A07G043_GPIO_RSTN>,
				 <&cpg R9A07G043_GPIO_PORT_RESETN>,
				 <&cpg R9A07G043_GPIO_SPARE_RESETN>;
		};

		plmt0: plmt0@110c0000 {
			compatible = "andestech,plmt0", "riscv,plmt0";
			reg = <0x00000000 0x110c0000 0x00000000 0x00100000>;
			interrupts-extended = <&cpu0_intc 7>;
		};

		plic: interrupt-controller@12c00000 {
			compatible = "renesas,r9a07g043-plic", "andestech,nceplic100";
			#interrupt-cells = <2>;
			#address-cells = <0>;
			riscv,ndev = <511>;
			interrupt-controller;
			reg = <0x0 0x12c00000 0 0x400000>;
			clocks = <&cpg CPG_MOD R9A07G043_NCEPLIC_ACLK>;
			power-domains = <&cpg>;
			resets = <&cpg R9A07G043_NCEPLIC_ARESETN>;
			interrupts-extended = <&cpu0_intc 11 &cpu0_intc 9>;
		};

		plicsw: interrupt-controller@13000000 {
			compatible = "andestech,plicsw";
			reg = <0x00000000 0x13000000 0x00000000 0x00400000>;
			interrupts-extended = <&cpu0_intc 3>;
			interrupt-controller;
			#address-cells = <2>;
			#interrupt-cells = <2>;
		};
	};
```
