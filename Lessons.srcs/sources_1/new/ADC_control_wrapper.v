module ad9235_ctrl_wrapper (
    input  wire        clk,          // system clock
    input  wire        rst_n,        // Asynchronous reset (active low)

    // --- Control Interface ---
    input  wire [15:0] sample_div,   // Clock divider value
    input  wire        enable,       // Enable module (1 = run, 0 = pause)

    // --- Physical ADC Interface (AD9235) ---
    output wire        adc_clk,      // Clock to the physical ADC pin
    input  wire [11:0] adc_data,     // 12-bit data from ADC
    input  wire        adc_otr,      // Out-of-Range flag from ADC

    // --- AXI-Stream Bridge Interface (to adc_to_axis) ---
    output wire [11:0] m_data,       // Captured ADC data
    output wire        m_valid,      // Data valid
    output wire        m_otr         // Out-of-Range flag
);

    ad9235_ctrl inst_ad9235_ctrl (
        .clk        (clk),
        .rst_n      (rst_n),
        .sample_div (sample_div),
        .enable     (enable),
        .adc_clk    (adc_clk),
        .adc_data   (adc_data),
        .adc_otr    (adc_otr),
        .m_data     (m_data),
        .m_valid    (m_valid),
        .m_otr      (m_otr)
    );

endmodule