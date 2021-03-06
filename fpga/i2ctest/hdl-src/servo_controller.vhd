library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;

entity servo_controller is
  generic (
    slave_address: std_logic_vector(6 downto 0) := (others => '0'));
  port (i2c_scl: inout std_logic;
        i2c_sda: inout std_logic;
        CLOCK_50: in std_logic;
        start: in std_logic;
        running: out std_logic;
        servo0: in unsigned(15 downto 0); --servo position
        servo1: in unsigned(15 downto 0) --( 0 - 4000 - 8000 )
      );
end servo_controller;


architecture synth of servo_controller is
  component i2c_master
    GENERIC(
      input_clk : INTEGER := 50_000_000; --input clock speed from user logic in Hz
      bus_clk   : INTEGER := 50_000);   --speed the i2c bus (scl) will run at in Hz
    PORT(
      clk       : IN     STD_LOGIC;                    --system clock
      reset_n   : IN     STD_LOGIC;                    --active low reset
      ena       : IN     STD_LOGIC;                    --latch in command
      addr      : IN     STD_LOGIC_VECTOR(6 DOWNTO 0); --address of target slave
      rw        : IN     STD_LOGIC;                    --'0' is write, '1' is read
      data_wr   : IN     STD_LOGIC_VECTOR(7 DOWNTO 0); --data to write to slave
      busy      : OUT    STD_LOGIC;                    --indicates transaction in progress
      data_rd   : OUT    STD_LOGIC_VECTOR(7 DOWNTO 0); --data read from slave
      ack_error : BUFFER STD_LOGIC;                    --flag if improper acknowledge from slave
      sda       : INOUT  STD_LOGIC;                    --serial data output of i2c bus
      scl       : INOUT  STD_LOGIC);                   --serial clock output of i2c bus
  end component;


  component edge_detector is
    port (
      CLK  : in  std_logic;
      A  : in  std_logic;
      R : out std_logic;
      F: out std_logic);
  end component edge_detector;

  type machine is (reset,started,preamble,command,arg1,arg2,finished);
  signal addr: std_logic_vector(6 downto 0) := slave_address;
  signal data_wr: std_logic_vector(7 downto 0) := (others => '0');
  signal datawrbuf: std_logic_vector(15 downto 0) := (others => '0');
  signal reset_n: std_logic := '0';
  signal ena: std_logic := '0';
  signal rw: std_logic := '0';
  signal busy: std_logic;
  signal ack_error: std_logic;
  signal state: machine := reset;
  signal busy_pulse: std_logic;

  signal servoid: std_logic_vector(2 downto 0) := "000";

begin
  i2c_servoboard: i2c_master
    generic map (bus_clk => 100_000)
    port map(
      clk => CLOCK_50,
      reset_n => reset_n,
      ena => ena,
      addr => addr,
      rw => rw,
      data_wr => data_wr,
      busy => busy,
      ack_error => ack_error,
      sda => i2c_sda,
      scl => i2c_scl);

  busy_edge: edge_detector
    port map(CLK => CLOCK_50, A => busy, R => busy_pulse);

  process(CLOCK_50)
  begin
    -- TODO: Actually use ack_error.
    --reset_n <= '1';
    --ena <= '0';
    --running <= '1';
    --addr <= (others => '0');
    --rw <= '0';
    --data_wr <= (others => '0');

    if rising_edge(CLOCK_50) then
      case state is
        when reset =>
          if start = '1' then
            state <= started;
            reset_n <= '1';                --enable the i2c component
          end if;
        when started =>
          --servoid <= "000";
          --datawrbuf <= conv_std_logic_vector(servo0,16);
          if servoid = "000" then
            servoid <= "001";
            datawrbuf <= conv_std_logic_vector(servo1,16);
          else
            servoid <= "000";
            datawrbuf <= conv_std_logic_vector(servo0,16);
          end if;
          addr <= slave_address;
          running <= '0';
          if busy = '0' then
            state <= preamble;
          end if;
        when preamble =>
          data_wr <= "11111111";
          ena <= '1';
          addr <= slave_address;
          if busy_pulse = '1' then
            state <= command;
          end if;
        when command =>
          data_wr <= "00000" & servoid;
          ena <= '1';
          addr <= slave_address;
          if busy_pulse = '1' then
            state <= arg1;
          end if;
        when arg1 =>
          if (datawrbuf(15 downto 8) = "11111111") or
             (datawrbuf(15 downto 8) = "11111110") then
            data_wr <= "11111101";
          else
            data_wr <= datawrbuf(15 downto 8);
          end if;
          ena <= '1';
          addr <= slave_address;
          if busy_pulse = '1' then
            state <= arg2;
          end if;
        when arg2 =>
          if (datawrbuf(7 downto 0) = "11111111") or
             (datawrbuf(7 downto 0) = "11111110") then
            data_wr <= "11111101";
          else
            data_wr <= datawrbuf(7 downto 0);
          end if;
          addr <= slave_address;
          ena <= '1';
          if busy_pulse = '1' then
            state <= finished;
          end if;
        when finished =>
          running <= '0';
          ena <= '0';
          if busy = '0' then
            state <= reset;
          end if;
      end case;
    end if;
  end process;

end synth;
