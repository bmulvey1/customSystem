library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity barrel_shifter_stage is
    generic (
        data_size : positive := 32;
        amount : positive := 0
    );
    port (
        data_in : in std_logic_vector(data_size - 1 downto 0);
        shift_amount : in std_logic;
        shift_type : in std_logic_vector(1 downto 0);
        data_out : out std_logic_vector(data_size - 1 downto 0);
        carry_out : out std_logic;
        carry_in : in std_logic
    );
end entity barrel_shifter_stage;

architecture behavioral of barrel_shifter_stage is
    signal data_temp : std_logic_vector(data_size - 1 downto 0);
begin
    process (shift_type, data_in)
    begin
        case shift_type is
            when "00" =>
                data_temp <= to_stdlogicvector(to_bitvector(data_in) sll amount);
            when "01" =>
                data_temp <= to_stdlogicvector(to_bitvector(data_in) srl amount);
            when "10" =>
                data_temp <= to_stdlogicvector(to_bitvector(data_in) sra amount);
            when "11" =>
                data_temp <= to_stdlogicvector(to_bitvector(data_in) ror amount);
            when others =>
                null;
        end case;
    end process;

    process (shift_amount, data_temp, data_in)
    begin
        if shift_amount = '1' then
            data_out <= data_temp;
        else
            data_out <= data_in;
        end if;
    end process;

    process (shift_type, data_in, carry_in)
    begin
        if carry_in = '1' then
            carry_out <= '1';
        elsif shift_type = "00" then
            carry_out <= data_in(31);
        else
            carry_out <= '0';
        end if;
    end process;

end architecture behavioral;