library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
entity RegFile is
    generic (
        data_size : positive := 8;
        address_size : positive := 4
    );
    port (
        clk : in std_logic;
        addr_in_A : in std_logic_vector(address_size - 1 downto 0);
        addr_in_B : in std_logic_vector(address_size - 1 downto 0);
        addr_in_C : in std_logic_vector(address_size - 1 downto 0);
        addr_in_D : in std_logic_vector(address_size - 1 downto 0);
        data_in_D : in std_logic_vector(data_size - 1 downto 0);
        WE : in std_logic;
        data_out_A : out std_logic_vector(data_size - 1 downto 0);
        data_out_B : out std_logic_vector(data_size - 1 downto 0);
        data_out_C : out std_logic_vector(data_size - 1 downto 0);
    );
end entity RegFile;
architecture Behavioral of RegFile is
    type RegFile_type is array(0 to 2 ** address_size - 1) of std_logic_vector(data_size - 1 downto 0);

    signal RegFile : RegFile_type := (others => (others => '0'));
begin
    process (clk)
    begin
        if rising_edge(clk) then
            if WE = '1' then
                RegFile(to_integer(unsigned(addr_in_D))) <= data_in_D;
            end if;
        end if;

    end process;

    process (addr_in_A)
    begin
        data_out_A <= RegFile(to_integer(unsigned(addr_in_A)));
    end process;

    process (addr_in_B)
    begin
        data_out_B <= RegFile(to_integer(unsigned(addr_in_B)));
    end process;

    process (addr_in_C)
    begin
        data_out_C <= RegFile(to_integer(unsigned(addr_in_C)));
    end process;
end architecture Behavioral;