`timescale 1ns / 1ps

module adc_to_axis #(
    parameter int FFT_LENGTH = 1024 // Typed parameter for FFT size
)(
    input  logic        clk,            // System clock
    input  logic        rst_n,          // Asynchronous reset (active low)

    // --- ADC Interface (Input) ---
    input  logic [11:0] adc_data,       // 12-bit ADC data
    input  logic        adc_valid,      // ADC data flag strobe

    // --- AXI4-Stream Interface (to Xilinx FFT IP) ---
    output logic [31:0] m_axis_tdata,   // I and Q channels
    output logic        m_axis_tvalid,  // Data valid flag
    output logic        m_axis_tlast,   // End of frame flag
    input  logic        m_axis_tready   // FFT ready flag
);

    logic [15:0] i_data;
    logic [15:0] q_data;

    //4xMSB times + 12 bits.
    assign i_data = { {4{adc_data[11]}}, adc_data };
    
    // Q is set to zero (no DDC yet).
    assign q_data = 16'd0;

    // AXI-STREAM BUS
    assign m_axis_tdata = {q_data, i_data};

    logic [15:0] sample_counter;
    logic        transfer_en;

    assign transfer_en = adc_valid & m_axis_tready;
    assign m_axis_tvalid = adc_valid;
    assign m_axis_tlast  = (sample_counter == (FFT_LENGTH - 1));

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            sample_counter <= 16'd0;
        end else begin
            if (transfer_en) begin
                if (m_axis_tlast) begin
                    sample_counter <= 16'd0;
                end else begin
                    sample_counter <= sample_counter + 1'b1;
                end
            end
        end
    end

endmodule