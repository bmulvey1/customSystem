library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;


entity RegFile is
    generic(
        data_size : positive := 16;
        address_size : positive := 5
    );
    port (
        clk : in std_logic;
        addr_in_A : in std_logic_vector(address_size-1 downto 0);
        addr_in_B : in std_logic_vector(address_size-1 downto 0);
        addr_in_C : in std_logic_vector(address_size-1 downto 0);
        data_in_C : in std_logic_vector(data_size-1 downto 0);
        WE : in std_logic;
        data_out_A : out std_logic_vector(data_size-1 downto 0);
        data_out_B : out std_logic_vector(data_size-1 downto 0)
    );
end entity RegFile;


architecture Behavioral of RegFile is
    type RegFile_type is array(0 to 2 ** address_size - 1) of std_logic_vector(data_size-1 downto 0);

    signal RegFile : RegFile_type := (others => (others => '0'));
begin
    

    process(clk)
    begin
        if rising_edge(clk) then
            if WE = '1' then
                RegFile(to_integer(unsigned(addr_in_C))) <= data_in_C;
            end if;
        end if;

    end process;

    process(addr_in_A)
    begin
        if addr_in_A = "00000" then
            data_out_A <= (others => '0');
        else
            data_out_A <= RegFile(to_integer(unsigned(addr_in_A)));
        end if;
    end process;

    process(addr_in_B)
    begin
        if addr_in_B = "00000" then
            data_out_B <= (others => '0');
        else
            data_out_B <= RegFile(to_integer(unsigned(addr_in_B)));
        end if;
    end process;
    
    
end architecture Behavioral;