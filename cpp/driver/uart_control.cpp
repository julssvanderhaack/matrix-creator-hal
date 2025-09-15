/*
 * Copyright 2016 <Admobilize>
 * MATRIX Labs  [http://creator.matrix.one]
 * This file is part of MATRIX Creator HAL
 *
 * MATRIX Creator HAL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <wiringPi.h>
#include <iostream>
#include <string>
#include <condition_variable>

#include "cpp/driver/creator_memory_map.h"
#include "cpp/driver/uart_control.h"

static std::mutex irq_m;
static std::condition_variable irq_cv;

void uart_irq_callback(void) { irq_cv.notify_all(); }

namespace matrix_hal
{

  const uint16_t kUartIRQ = 5;
  const uint16_t UART_BUSY = 0x0010;

  /* // Old code using deprecated features
  uint16_t UartControl::GetUartValue()
  {
    if (!bus_)
      return false;
    uint16_t value;
    // Wait forever, so from there this function returns false if there is an error.
    if (waitForInterrupt(kUartIRQ, -1) > 0)
    {
      bus_->Read(kUartBaseAddress + 1, &value);
      return value;
    }
    return false;
  }

  void UartControl::Setup(MatrixIOBus *bus)
  {
    MatrixDriver::Setup(bus);
    // TODO(andres.calderon@admobilize.com): avoid systems calls
    // FIXME: Remove the gpio command, it doesn't work on new kernels
    // It can be modeled after the microphones code, by using a
    // condition variable and firing an interrupt.
    // See microphone_array.cpp
    int status = std::system("gpio edge 5 rising");
    if (status != 0)
    {
      std::cout << "Error executing gpio command" << std::endl;
    }

    wiringPiSetupSys();

    pinMode(kUartIRQ, INPUT);
  }
  */

  uint16_t UartControl::GetUartValue()
  {
    if (!bus_)
    {
      return false;
    }
    uint16_t value;

    irq_cv.wait(lock_);

    // Return the read value if there is a sucefull read.
    if (bus_->Read(kUartBaseAddress + 1, &value))
    {
      return value;
    }
    // Return false if thre is an error.
    return false;
  }

  bool UartControl::GetUartUCR()
  {
    if (!bus_)
      return false;
    uint16_t value;
    bus_->Read(kUartBaseAddress, &value);
    ucr_ = value;
    return true;
  }

  bool UartControl::SetUartValue(uint16_t data)
  {
    if (!bus_)
      return false;
    do
    {
      GetUartUCR();
    } while (ucr_ & UART_BUSY);
    bus_->Write(kUartBaseAddress + 1, data);
    return true;
  }

  UartControl::UartControl() : lock_(irq_m), ucr_(0x0) {}

  void UartControl::Setup(MatrixIOBus *bus)
  {
    MatrixDriver::Setup(bus);

    wiringPiSetupSys();

    pinMode(kUartIRQ, INPUT);
    wiringPiISR(kUartIRQ, INT_EDGE_BOTH, &uart_irq_callback);
  }
} // namespace matrix_hal
