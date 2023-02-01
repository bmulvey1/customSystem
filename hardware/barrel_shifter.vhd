library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
use IEEE.math_real.all;

entity barrel_shifter is
    generic (data_size : positive := 32);
    port (
        data_in : in std_logic_vector(data_size - 1 downto 0);
        shift_type : in std_logic_vector(1 downto 0);
        shift_amount : in std_logic_vector(natural(ceil(log2(real(data_size)))) - 1 downto 0);
        data_out : out std_logic_vector(data_size - 1 downto 0);
        carry_out : out std_logic
    );

    attribute dont_touch : string; -- TODO: REMOVE ME
    attribute dont_touch of barrel_shifter : entity is "yes"; -- TODO: REMOVE ME

end entity barrel_shifter;

architecture behavioral of barrel_shifter is

    component barrel_shifter_stage
        generic (
            data_size : positive := 32;
            amount : positive := 0);
        port (
            data_in : in std_logic_vector(data_size - 1 downto 0);
            shift_amount : in std_logic;
            shift_type : in std_logic_vector(1 downto 0);
            data_out : out std_logic_vector(data_size - 1 downto 0);
            carry_out : out std_logic;
            carry_in : in std_logic
        );
    end component;
    attribute dont_touch of barrel_shifter_stage : component is "yes"; -- TODO: REMOVE ME

    type data_array is array(0 to natural(ceil(log2(real(data_size)))) - 1) of std_logic_vector(data_size - 1 downto 0);
    signal data : data_array;
    signal carry : std_logic_vector(natural(ceil(log2(real(data_size)))) - 1 downto 0);

begin
    carry(natural(ceil(log2(real(data_size)))) - 1) <= '0';
    data(natural(ceil(log2(real(data_size)))) - 1) <= data_in;

    gen : for i in natural(ceil(log2(real(data_size)))) - 1 downto 1 generate
        stage : barrel_shifter_stage
        generic map(data_size => data_size, amount => 2 ** i)
        port map(data_in => data(i), shift_amount => shift_amount(i), shift_type => shift_type, data_out => data(i - 1), carry_out => carry(i - 1), carry_in => carry(i));

    end generate;

    stage0 : barrel_shifter_stage
    generic map(data_size => data_size, amount => 1)
    port map(data_in => data(0), shift_amount => shift_amount(0), shift_type => shift_type, data_out => data_out, carry_out => carry_out, carry_in => carry(0));

end architecture behavioral;