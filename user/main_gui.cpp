//---------------------------------------------------------------
//
//  m a i n _ g u i . c p p
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#include <stdio.h>
#include <string.h> // memset()
#include <unistd.h> // usleep()
#include <signal.h>

#include <fltk/Window.h>
#include <fltk/Slider.h>
#include <fltk/ValueOutput.h>
#include <fltk/FillSlider.h>
#include <fltk/CheckButton.h>
#include <fltk/run.h>
using namespace fltk;

#include "ec_globals.h"
#include "ec_master.h"

#define SLIDER_UPDATE_CYCLE 0.02
#define VALUES_UPDATE_CYCLE 0.50

//---------------------------------------------------------------

unsigned short int write_value;
signed short int read_value;
unsigned char dig_value;

void write_data(unsigned char *);
void read_data(unsigned char *);

void slider_write_callback(Widget *, void *);
void slider_read_timeout(void *);
void values_timeout(void *);

Window *window;
Slider *slider_read, *slider_write;
ValueOutput *output_cycle, *output_jitter, *output_work, *output_busy, *output_bus;
CheckButton *check1, *check2, *check3, *check4;
EtherCAT_master_t master;

double max_cycle, max_jitter, max_work, max_busy, max_bus;

//---------------------------------------------------------------

#define SLAVE_COUNT 7

EtherCAT_slave_t slaves[SLAVE_COUNT] =
{
  ECAT_INIT_SLAVE(Beckhoff_EK1100),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL1014),
  ECAT_INIT_SLAVE(Beckhoff_EL5001),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL3102)
};

//---------------------------------------------------------------

int main(int argc, char **argv)
{
  //unsigned int i;
  EtherCAT_slave_t *buskoppler, *input, *output, *dig_in, *dig_out;
  struct sched_param sched;

  printf("CatEther-Testprogramm.\n\n");

  //----------

#if 1
  printf("Setting highest task priority...\n");

  sched.sched_priority = sched_get_priority_max(SCHED_RR);
  if (sched_setscheduler(0, SCHED_RR, &sched) == -1)
  {
    fprintf(stderr, "ERROR: Could not set priority: %s\n", strerror(errno)); 
    return -1;
  }
#endif

  //----------

  printf("Initializing master...\n");
  EtherCAT_master_init(&master, "eth1");

  printf("Checking slaves...\n");
  if (EtherCAT_check_slaves(&master, slaves, SLAVE_COUNT) != 0)
  {
    fprintf(stderr, "ERROR while searching for slaves!\n");
    return -1;
  }

  //----------

  // Check for slaves

  buskoppler = &slaves[0];
  output = &slaves[1];
  dig_in = &slaves[3];
  dig_out = &slaves[5];
  input = &slaves[6];

  // Set Mapping addresses

  output->logical_address0 = 0x00000000;
  input->logical_address0 = 0x00000004;
  dig_in->logical_address0 = 0x0000000F;
  dig_out->logical_address0 = 0x0000000E;

  //----------

  printf("Init output slave...\n");

  if (EtherCAT_activate_slave(&master, output) != 0)
  {
    fprintf(stderr, "ERROR: Could not init slave!\n");
    return -1;
  }

  printf("Init input slave...\n");

  if (EtherCAT_activate_slave(&master, input) != 0)
  {
    fprintf(stderr, "ERROR: Could not init slave!\n");
    return -1;
  }

  printf("Init digital input slave...\n");

  if (EtherCAT_activate_slave(&master, dig_in) != 0)
  {
    fprintf(stderr, "ERROR: Could not init slave!\n");
    return -1;
  }

  printf("Init digital output slave...\n");

  if (EtherCAT_activate_slave(&master, dig_out) != 0)
  {
    fprintf(stderr, "ERROR: Could not init slave!\n");
    return -1;
  }

  //----------

  printf("Starting FLTK window...\n");
  
  window = new Window(300, 300);
  window->begin();

  slider_read = new FillSlider(50, 10, 40, 280);
  slider_read->set_vertical();
  slider_read->buttoncolor(BLUE);
  
  slider_read->deactivate();

  slider_write = new Slider(110, 10, 40, 280);
  slider_write->set_vertical();
  slider_write->callback(slider_write_callback, NULL);

  output_cycle = new ValueOutput(200, 50, 90, 25, "Cycle time [µs]");
  output_cycle->align(ALIGN_LEFT | ALIGN_TOP);

  output_jitter = new ValueOutput(200, 90, 90, 25, "Jitter [%]");
  output_jitter->align(ALIGN_LEFT | ALIGN_TOP);

  output_work = new ValueOutput(200, 130, 90, 25, "Work time [µs]");
  output_work->align(ALIGN_LEFT | ALIGN_TOP);

  output_busy = new ValueOutput(200, 170, 90, 25, "Busy rate [%]");
  output_busy->align(ALIGN_LEFT | ALIGN_TOP);

  output_bus = new ValueOutput(200, 210, 90, 25, "Bus time [µs]");
  output_bus->align(ALIGN_LEFT | ALIGN_TOP);

  check1 = new CheckButton(200, 240, 30, 25, "1");
  check2 = new CheckButton(250, 240, 30, 25, "2");
  check3 = new CheckButton(200, 270, 30, 25, "3");
  check4 = new CheckButton(250, 270, 30, 25, "4");

  //  output_cycle = new Output(200, 35, 90, 25);

  window->end();
  window->show();

  add_timeout(SLIDER_UPDATE_CYCLE, slider_read_timeout, NULL);
  add_timeout(VALUES_UPDATE_CYCLE, values_timeout, NULL);

  printf("Starting thread...\n");

  if (EtherCAT_start(&master, 20, write_data, read_data, 10000) != 0)
  {
    return -1;
  }

  run(); // Start FLTK loop

  remove_timeout(slider_read_timeout, NULL);
  remove_timeout(values_timeout, NULL);

  printf("Stopping master thread...\n");
  EtherCAT_stop(&master);

  printf("Deactivating slaves...\n");

  EtherCAT_deactivate_slave(&master, dig_out);
  EtherCAT_deactivate_slave(&master, dig_in);
  EtherCAT_deactivate_slave(&master, input);
  EtherCAT_deactivate_slave(&master, output);
  EtherCAT_deactivate_slave(&master, buskoppler);

  EtherCAT_master_clear(&master);

  printf("Finished.\n");
  
  return 0;
}

//---------------------------------------------------------------

void write_data(unsigned char *data)
{
  data[0] = write_value & 0xFF;
  data[1] = (write_value & 0xFF00) >> 8;

  data[14] = (write_value * 16 / 32767) & 0x0F;
}

//---------------------------------------------------------------

void read_data(unsigned char *data)
{
  read_value = data[5] | data[6] << 8;
  dig_value = data[15];
}

//---------------------------------------------------------------

void slider_read_timeout(void *data)
{
  slider_read->value((double) read_value / 65536 + 0.5);
  slider_read->redraw();

  check1->value(dig_value & 1);
  check2->value(dig_value & 2);
  check3->value(dig_value & 4);
  check4->value(dig_value & 8);

  if (max_cycle < master.last_cycle_time) max_cycle = master.last_cycle_time;
  if (max_jitter < master.last_jitter) max_jitter = master.last_jitter;
  if (max_work < master.last_cycle_work_time) max_work = master.last_cycle_work_time;
  if (max_busy < master.last_cycle_busy_rate) max_busy = master.last_cycle_busy_rate;
  if (max_bus < master.bus_time) max_bus = master.bus_time;

  repeat_timeout(SLIDER_UPDATE_CYCLE, slider_read_timeout, NULL);
}

//---------------------------------------------------------------

void values_timeout(void *data)
{
  output_cycle->value(max_cycle * 1000000.0);
  output_jitter->value(max_jitter);
  output_work->value(max_work * 1000000.0);
  output_busy->value(max_busy);
  output_bus->value(max_bus * 1000000.0);

  max_cycle = max_jitter = max_work = max_busy = max_bus = 0.0;

  repeat_timeout(VALUES_UPDATE_CYCLE, values_timeout, NULL);
}

//---------------------------------------------------------------

void slider_write_callback(Widget *sender, void *data)
{
  write_value = (short unsigned int) (32767 * slider_write->value() + 0.5);
}

//---------------------------------------------------------------
