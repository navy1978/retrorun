#pragma once
/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride
Copyright (C) 2023-present  navy1978

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
typedef enum 
{
    Go2HardwareRevision_Unknown = 0,
    Go2HardwareRevision_V1_0,
    Go2HardwareRevision_V1_1
} go2_hardware_revision_t;


go2_hardware_revision_t go2_hardware_revision_get();