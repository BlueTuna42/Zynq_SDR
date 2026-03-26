module ad9235_ctrl (
    input  logic        clk,          // system clock
    input  logic        rst_n,        // Asynchronous reset (active low)

    // --- Control Interface ---
    input  logic [15:0] sample_div,   // Clock divider value
    input  logic        enable,       // Enable module (1 = run, 0 = pause)

    // --- Physical ADC Interface (AD9235) ---
    output logic        adc_clk,  // Сlock to the physical ADC pin
    input  logic [11:0] adc_data,  // 12-bit data from ADC
    input  logic        adc_otr,   // Out-of-Range flag from ADC

    // --- AXI-Stream Bridge Interface (to adc_to_axis) ---
    output logic [11:0] m_data,       // Captured ADC data
    output logic        m_valid,      // Data valid
    output logic        m_otr         // Out-of-Range flag
);

    logic [15:0] clk_counter;
    logic        clk_toggle;
    logic        capture_tick;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            clk_counter  <= '0;
            clk_toggle   <= 1'b0;
            capture_tick <= 1'b0;
        end else if (enable) begin
            if (clk_counter >= (sample_div - 1'b1)) begin
                clk_counter  <= '0;
                clk_toggle   <= ~clk_toggle;
                
                // Generate capture strobe on the falling edge of the ADC clock AD9235 captures data on the rising edge, so falling edge is stable.
                capture_tick <= clk_toggle; 
            end else begin
                clk_counter  <= clk_counter + 1'b1;
                capture_tick <= 1'b0;
            end
        end else begin
            clk_counter  <= '0;
            clk_toggle   <= 1'b0;
            capture_tick <= 1'b0;
        end
    end

    // Clock Forwarding using Xilinx ODDR primitive
    // This ensures minimum jitter and aligns the output clock with the FPGA's I/O logic.
    ODDR #(
        .DDR_CLK_EDGE("OPPOSITE_EDGE"), // Default edge alignment
        .INIT(1'b0),                    // Initial value of Q
        .SRTYPE("SYNC")                 // Synchronous set/reset
    ) ODDR_adc_clk (
        .Q(adc_clk_out),                // Forwarded clock to the output pin
        .C(clk_toggle),                 // Internal generated clock
        .CE(1'b1),                      // Clock enable (always on)
        .D1(1'b1),                      // Output high on positive edge
        .D2(1'b0),                      // Output low on negative edge
        .R(1'b0),                       // Reset (unused)
        .S(1'b0)                        // Set (unused)
    );

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            m_data  <= '0;
            m_otr   <= 1'b0;
            m_valid <= 1'b0;
        end else begin
            m_valid <= capture_tick; 
            
            if (capture_tick) begin
                m_data <= adc_data;
                m_otr  <= adc_otr;
            end
        end
    end

endmodule