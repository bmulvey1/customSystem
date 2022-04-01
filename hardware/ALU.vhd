library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity ALU is
    generic(data_size : positive := 16)
    port (
        A, B : in std_logic_vector(data_size-1 downto 0);
        ALUControl : in std_logic_vector(3 downto 0);
        ci : in std_logic;
        Result : out std_logic_vector(data_size-1 downto 0);
        ALUFlags : out std_logic_vector(3 downto 0); -- NZCV
    );
end entity ALU;

architecture Behavioral of ALU is
    signal N, Z, C, V : std_logic;
    signal add_i : unsigned(data_size downto 0);
    signal add_b : std_logic_vector(data_size-1 downto 0) := (others => '0');
    signal result_i : unsigned(data_size-1 downto 0);
    signal Cout : std_logic;
begin
    
    process(ALUControl, A, B, add_i)
    begin
        case ALUControl is
            when "0000" => 
                add_b <= B;
                add_i <= resize(unsigned(A), data_size+1) + resize(unsigned(add_b), data_size+1);
                result_i <= add_i(data_size-1 downto 0);
                Cout <= add_i(data_size);
            when "0001" => 
                add_b <= not B;
                add_i <= resize(unsigned(A), data_size+1) + resize(unsigned(add_b), data_size+1);
                result_i <= add_i(data_size-1 downto 0);
                Cout <= add_i(data_size);
            when "0010" => 
                result_i <= A * B; -- this should work
            when "0011" => 
                result_i <= A / B; -- this probably won't work
            when "0100" => 
                -- later
            when "0101" => 
                -- later
            when "0110" => 
                result_i <= A and B;
            when "0111" => 
                result_i <= A or B;
            when "1000" => 
                result_i <= A xor B;
            when "1001" => 
                result_i <= not A;
            when others => 
                null;
        end case;
    end process;
    
end architecture Behavioral;