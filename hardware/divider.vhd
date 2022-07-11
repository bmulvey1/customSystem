library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;


entity divider is
    generic(data_size : positive := 16);
    port (
        A, B : in std_logic_vector(data_size downto 0); -- a / b
        r : out std_logic_vector(data_size downto 0)
    );
end entity divider;

architecture rtl of divider is
    
begin
    
    process(A, B)
    begin
        if unsigned(A) = unsigned(B) then
            r <= std_logic_vector(1);
        elsif unsigned(A) < unsigned(B) then
            if unsigned(B) < 2*unsigned(A) then
                r <= std_logic_vector(1);
            else
                r <= std_logic_vector(0);
            end if;
        else
            for i in 1 to 16 loop
                if unsigned(to_bitvector(A) sll i) = unsigned(B) then
                    r <= std_logic_vector(2 ** i);
                end if;
            end loop;
        end if;
    end process;    
    
    
end architecture rtl;