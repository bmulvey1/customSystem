library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity ALU is
    generic(data_size : positive := 16);
    port (
        A, B : in std_logic_vector(data_size-1 downto 0);
        ALUControl : in std_logic_vector(2 downto 0);
        ci : in std_logic;
        Result : out std_logic_vector(data_size-1 downto 0);
        ALUFlags : out std_logic_vector(3 downto 0) -- NZCV
        );
end entity ALU;

architecture Behavioral of ALU is
    signal N, Z, C, V : std_logic;
    signal add_i : unsigned(data_size downto 0);
    signal add_b : std_logic_vector(data_size-1 downto 0) := (others => '0');
    signal mult_i : unsigned(2*data_size - 1 downto 0) := (others => '0');
    signal result_i : unsigned(data_size-1 downto 0);
    signal Cout : std_logic;
begin
    
    process(ALUControl, A, B, add_i, add_b, mult_i)
    begin
        case ALUControl is
            when "000" => 
                add_b <= B;
                add_i <= resize(unsigned(A), data_size+1) + resize(unsigned(add_b), data_size+1);
                result_i <= add_i(data_size-1 downto 0);
                Cout <= add_i(data_size);
            when "001" => 
                add_b <= not B;
                add_i <= resize(unsigned(A), data_size+1) + resize(unsigned(add_b), data_size+1) + 1;
                result_i <= add_i(data_size-1 downto 0);
                Cout <= add_i(data_size);
            when "010" => 
                mult_i <= (unsigned(A) * unsigned(B)); -- this should work
                result_i <= mult_i(data_size-1 downto 0);
            when "011" => 
                result_i <= unsigned(A) / unsigned(B); -- this probably won't work
            when "100" => 
                result_i <= unsigned(A and B);
            when "101" => 
                result_i <= unsigned(A or B);
            when "110" => 
                result_i <= unsigned(A xor B);
            when "111" => 
                result_i <= unsigned(not A);
            when others => 
                null;
        end case;
    end process;

    N <= result_i(data_size-1);
    Z <= '1' when (result_i = (result_i'range => '0')) else '0';
    
    process(ALUControl, Cout, ci)
    begin
        if ALUControl = "000" or ALUControl = "001" then
            C <= Cout;
        else
            C <= ci;
        end if;
    end process;
    V <= (not ALUControl(1) and not(ALUControl(2))) and (add_i(data_size - 1) xor A(data_size - 1)) and not (ALUControl(0) xor A(data_size - 1) xor B(data_size - 1)) and not (mult_i(2 * data_size - 1 downto data_size) = x"0000"); -- this is weird

    ALUFlags <= N & Z & C & V;
    Result <= std_logic_vector(result_i);
    
    
end architecture Behavioral;