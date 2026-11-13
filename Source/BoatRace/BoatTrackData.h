#pragma once

// Generated and checked by Scripts/course_layout.py: gate lines never cross,
// every obstacle clears the gate lines and the other obstacles, driving the
// racing line scores all 16 gates per lap in order, and driving it backwards
// scores none.

// Distances are metres. BoatTrack.cpp converts to Unreal units (1 uu = 1 cm).

namespace BoatTrackData
{
	// Half the course width. 90 m is the UIM minimum for an F1H2O circuit.
	const double GateHalfWidthMetres = 45.0;

	// Racing line, counter-clockwise. Index 0 is the start / finish line.
	const double RacingLine[][2] =
	{
		{      0.00,   -480.00 },
		{    210.00,   -480.00 },
		{    395.00,   -425.00 },
		{    520.00,   -295.00 },
		{    560.00,   -110.00 },
		{    525.00,     70.00 },
		{    400.00,    135.00 },
		{    215.00,    150.00 },
		{     75.00,    275.00 },
		{   -105.00,    330.00 },
		{   -300.00,    315.00 },
		{   -435.00,    195.00 },
		{   -495.00,     25.00 },
		{   -465.00,   -175.00 },
		{   -350.00,   -335.00 },
		{   -180.00,   -450.00 },
	};

	// Rock pillars: X, Y, radius
	const double Rocks[][3] =
	{
		{    286.98,   -434.16,     12.00 },
		{    476.24,   -378.02,     10.00 },
		{    518.54,   -188.18,      9.00 },
		{    487.92,    120.84,     11.00 },
		{    127.68,    193.11,     10.00 },
		{   -194.59,    347.18,      9.00 },
		{   -441.43,    101.68,     12.00 },
		{   -435.17,   -262.76,     10.00 },
		{   -243.61,   -379.20,      8.00 },
	};

	// Jump ramps: X, Y, heading in degrees
	const double Ramps[][3] =
	{
		{    115.50,   -462.00,      0.00 },
		{    562.13,    -16.18,    101.00 },
		{    -20.26,    285.29,    163.01 },
		{   -496.30,    -87.67,    -81.47 },
	};

	// Whirlpools: X, Y, radius
	const double Whirlpools[][3] =
	{
		{    421.46,   -325.34,     30.00 },
		{    311.54,    192.34,     28.00 },
		{   -334.28,    217.63,     30.00 },
	};

	// Ammo crates: X, Y
	const double Crates[][2] =
	{
		{    168.00,   -488.00 },
		{    419.04,   -385.57 },
		{    543.57,     26.91 },
		{    421.31,    114.90 },
		{    108.99,    256.71 },
		{   -250.56,    309.78 },
		{   -490.54,     61.66 },
		{   -301.90,   -355.47 },
	};

	// Floating debris barrels: X, Y
	const double Debris[][2] =
	{
		{    237.31,   -436.41 },
		{    357.87,   -469.42 },
		{    502.68,   -233.16 },
		{    584.62,    -58.51 },
		{    323.33,    108.11 },
		{    245.17,    177.65 },
		{     29.94,    256.35 },
		{   -146.53,    359.90 },
		{   -359.74,    219.08 },
		{   -518.16,    -29.60 },
		{   -360.14,   -269.49 },
		{   -113.43,   -494.55 },
	};

	// Floating logs: X, Y, heading in degrees
	const double Logs[][3] =
	{
		{     63.00,   -510.00,      0.00 },
		{    580.30,   -162.80,     77.80 },
		{    461.16,     69.38,    152.53 },
		{    201.31,    205.12,    138.24 },
		{   -420.77,    142.18,   -109.44 },
		{   -467.17,   -225.09,    -54.29 },
		{    -40.07,   -442.91,     -9.46 },
	};

	// Shore per racing-line point: water reach inside, outside (m), cliff inside, outside (0..1)
	const double Shore[][4] =
	{
		{    105.00,    140.00,      0.00,      0.00 },
		{    100.00,    130.00,      0.10,      0.00 },
		{    110.00,    120.00,      0.30,      0.40 },
		{    115.00,    110.00,      0.50,      0.90 },
		{    100.00,    115.00,      0.40,      1.00 },
		{     95.00,    110.00,      0.50,      0.90 },
		{    105.00,    120.00,      0.80,      0.30 },
		{    100.00,    100.00,      0.90,      0.20 },
		{     62.00,     64.00,      1.00,      0.80 },
		{     63.00,     62.00,      1.00,      0.90 },
		{    100.00,     95.00,      0.40,      0.60 },
		{    110.00,    105.00,      0.20,      0.90 },
		{    100.00,    110.00,      0.10,      1.00 },
		{     95.00,    100.00,      0.00,      0.80 },
		{     70.00,     72.00,      0.20,      0.40 },
		{     80.00,     95.00,      0.10,      0.10 },
	};

	const double BargeChannelMetres = 34.0;
	const double BargeOverrunMetres = 30.0;

	// Moving barges: from X, Y, to X, Y, round-trip seconds
	const double Barges[][5] =
	{
		{    340.00,   -230.00,    640.00,   -230.00,     44.00 },
		{   -370.00,   -140.00,   -640.00,   -140.00,     52.00 },
	};

}
