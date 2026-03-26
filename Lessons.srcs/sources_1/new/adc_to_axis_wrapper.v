module adc_to_axis_wrapper #(
    parameter integer FFT_LENGTH = 1024
)(
    input wire        clk,
    input wire        rst_n,
    
    // ADC Interface
    input wire [11:0] adc_data,
    input wire        adc_valid,
    
    // AXI4-Stream Interface
    output wire [31:0] m_axis_tdata,
    output wire        m_axis_tvalid,
    output wire        m_axis_tlast,
    input wire         m_axis_tready
);

    adc_to_axis #(
        .FFT_LENGTH(FFT_LENGTH)
    ) inst_adc_to_axis (
        .clk           (clk),
        .rst_n         (rst_n),
        .adc_data      (adc_data),
        .adc_valid     (adc_valid),
        .m_axis_tdata  (m_axis_tdata),
        .m_axis_tvalid (m_axis_tvalid),
        .m_axis_tlast  (m_axis_tlast),
        .m_axis_tready (m_axis_tready)
    );

endmodule