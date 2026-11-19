#pragma once


// Generated and checked by Scripts/course_layout.py: gate lines never cross,
// every obstacle clears the gate lines and the other obstacles, driving the
// racing line scores all 16 gates per lap in order, and driving it backwards
// scores none.

// Distances are metres. BoatTrack.cpp converts to Unreal units (1 uu = 1 cm).

namespace BoatCourse0
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



// Generated and checked by Scripts/course_layout.py: gate lines never cross,
// every obstacle clears the gate lines and the other obstacles, driving the
// racing line scores all 16 gates per lap in order, and driving it backwards
// scores none.

// Distances are metres. BoatTrack.cpp converts to Unreal units (1 uu = 1 cm).

namespace BoatCourse1
{
	// Half the course width. 90 m is the UIM minimum for an F1H2O circuit.
	const double GateHalfWidthMetres = 45.0;

	// Racing line, counter-clockwise. Index 0 is the start / finish line.
	const double RacingLine[][2] =
	{
		{      0.00,   -360.00 },
		{    170.00,   -360.00 },
		{    330.00,   -330.00 },
		{    440.00,   -220.00 },
		{    465.00,    -65.00 },
		{    420.00,     90.00 },
		{    300.00,    180.00 },
		{    135.00,    185.00 },
		{      0.00,    200.00 },
		{   -110.00,    265.00 },
		{   -260.00,    250.00 },
		{   -390.00,    150.00 },
		{   -420.00,    -10.00 },
		{   -380.00,   -180.00 },
		{   -270.00,   -290.00 },
		{   -135.00,   -350.00 },
	};

	// Rock pillars: X, Y, radius
	const double Rocks[][3] =
	{
		{    237.95,   -324.88,     12.00 },
		{    403.38,   -293.38,     10.00 },
		{    430.06,   -130.93,      9.00 },
		{    388.80,    148.40,     11.00 },
		{     64.63,    166.66,     10.00 },
		{   -179.89,    282.13,      9.00 },
		{   -380.43,     65.39,     12.00 },
		{   -349.59,   -248.59,     10.00 },
		{   -186.41,   -301.98,      8.00 },
	};

	// Jump ramps: X, Y, heading in degrees
	const double Ramps[][3] =
	{
		{     93.50,   -342.00,      0.00 },
		{    461.71,     18.08,    106.19 },
		{    -64.16,    217.00,    149.42 },
		{   -415.52,   -107.62,    -76.76 },
	};

	// Whirlpools: X, Y, radius
	const double Whirlpools[][3] =
	{
		{    349.64,   -239.64,     30.00 },
		{    219.01,    232.48,     28.00 },
		{   -294.51,    160.37,     30.00 },
	};

	// Ammo crates: X, Y
	const double Crates[][2] =
	{
		{    136.00,   -368.00 },
		{    350.43,   -295.43 },
		{    440.85,     54.04 },
		{    319.20,    155.60 },
		{     27.99,    205.94 },
		{   -221.60,    244.79 },
		{   -421.86,     23.47 },
		{   -232.19,   -295.86 },
	};

	// Floating debris barrels: X, Y
	const double Debris[][2] =
	{
		{    195.73,   -320.58 },
		{    295.90,   -368.95 },
		{    417.88,   -168.72 },
		{    486.40,    -16.77 },
		{    233.00,    149.02 },
		{    160.66,    214.24 },
		{    -37.77,    186.31 },
		{   -143.28,    294.84 },
		{   -318.49,    164.64 },
		{   -440.18,    -59.60 },
		{   -281.79,   -235.79 },
		{    -83.44,   -386.91 },
	};

	// Floating logs: X, Y, heading in degrees
	const double Logs[][3] =
	{
		{     51.00,   -390.00,      0.00 },
		{    489.35,   -108.69,     80.84 },
		{    354.00,    102.00,    143.13 },
		{    104.78,    220.55,    173.66 },
		{   -367.03,    104.29,   -100.62 },
		{   -379.92,   -223.92,    -45.00 },
		{    -31.53,   -327.58,     -4.24 },
	};

	// Shore per racing-line point: water reach inside, outside (m), cliff inside, outside (0..1)
	const double Shore[][4] =
	{
		{    105.00,    140.00,      0.00,      0.00 },
		{    100.00,    130.00,      0.03,      0.00 },
		{    110.00,    120.00,      0.09,      0.12 },
		{    115.00,    110.00,      0.15,      0.27 },
		{    100.00,    115.00,      0.12,      0.30 },
		{     95.00,    110.00,      0.15,      0.27 },
		{    105.00,    120.00,      0.24,      0.09 },
		{    100.00,    100.00,      0.27,      0.06 },
		{     62.00,     64.00,      0.30,      0.24 },
		{     63.00,     62.00,      0.30,      0.27 },
		{    100.00,     95.00,      0.12,      0.18 },
		{    110.00,    105.00,      0.06,      0.27 },
		{    100.00,    110.00,      0.03,      0.30 },
		{     95.00,    100.00,      0.00,      0.24 },
		{     70.00,     72.00,      0.06,      0.12 },
		{     80.00,     95.00,      0.03,      0.03 },
	};

	const double BargeChannelMetres = 34.0;
	const double BargeOverrunMetres = 30.0;

	// Moving barges: from X, Y, to X, Y, round-trip seconds
	const double Barges[][5] =
	{
		{    542.47,   -212.67,    345.03,   -180.83,     44.00 },
		{   -485.34,   -168.90,   -290.66,   -123.10,     52.00 },
	};

}



// Generated and checked by Scripts/course_layout.py: gate lines never cross,
// every obstacle clears the gate lines and the other obstacles, driving the
// racing line scores all 16 gates per lap in order, and driving it backwards
// scores none.

// Distances are metres. BoatTrack.cpp converts to Unreal units (1 uu = 1 cm).

namespace BoatCourse2
{
	// Half the course width. 90 m is the UIM minimum for an F1H2O circuit.
	const double GateHalfWidthMetres = 45.0;

	// Racing line, counter-clockwise. Index 0 is the start / finish line.
	const double RacingLine[][2] =
	{
		{      0.00,   -490.00 },
		{    230.00,   -480.00 },
		{    440.00,   -395.00 },
		{    570.00,   -235.00 },
		{    580.00,    -25.00 },
		{    480.00,    150.00 },
		{    290.00,    220.00 },
		{    140.00,    180.00 },
		{     10.00,    280.00 },
		{   -160.00,    390.00 },
		{   -370.00,    360.00 },
		{   -540.00,    210.00 },
		{   -570.00,      0.00 },
		{   -510.00,   -210.00 },
		{   -360.00,   -390.00 },
		{   -180.00,   -470.00 },
	};

	// Rock pillars: X, Y, radius
	const double Rocks[][3] =
	{
		{    316.25,   -421.36,     12.00 },
		{    525.18,   -331.40,     10.00 },
		{    551.53,   -118.36,      9.00 },
		{    413.68,    204.27,     11.00 },
		{     59.15,    209.39,     10.00 },
		{   -257.89,    400.26,      9.00 },
		{   -530.25,    101.46,     12.00 },
		{   -463.24,   -308.28,     10.00 },
		{   -251.66,   -412.98,      8.00 },
	};

	// Jump ramps: X, Y, heading in degrees
	const double Ramps[][3] =
	{
		{    125.72,   -466.52,      2.49 },
		{    547.36,     72.42,    119.74 },
		{    -84.78,    319.89,    147.09 },
		{   -554.31,   -120.44,    -74.05 },
	};

	// Whirlpools: X, Y, radius
	const double Whirlpools[][3] =
	{
		{    466.19,   -283.47,     30.00 },
		{    202.12,    248.31,     28.00 },
		{   -421.92,    247.51,     30.00 },
	};

	// Ammo crates: X, Y
	const double Crates[][2] =
	{
		{    184.35,   -489.99 },
		{    464.74,   -348.69 },
		{    513.68,    111.21 },
		{    325.23,    198.49 },
		{     41.49,    267.13 },
		{   -316.23,    358.59 },
		{   -571.92,     43.13 },
		{   -310.94,   -400.86 },
	};

	// Floating debris barrels: X, Y
	const double Debris[][2] =
	{
		{    259.24,   -431.48 },
		{    399.51,   -445.91 },
		{    543.03,   -170.57 },
		{    584.52,     35.62 },
		{    238.50,    172.11 },
		{    184.77,    222.99 },
		{    -40.84,    275.97 },
		{   -206.67,    416.67 },
		{   -450.83,    246.01 },
		{   -584.81,    -61.02 },
		{   -381.95,   -316.79 },
		{   -111.64,   -510.80 },
	};

	// Floating logs: X, Y, heading in degrees
	const double Logs[][3] =
	{
		{     70.30,   -516.97,      2.49 },
		{    608.46,    -78.97,     87.27 },
		{    393.63,    149.85,    159.78 },
		{    127.01,    230.36,    142.43 },
		{   -516.81,    153.12,    -98.13 },
		{   -503.81,   -265.85,    -50.19 },
		{    -41.69,   -455.18,     -6.34 },
	};

	// Shore per racing-line point: water reach inside, outside (m), cliff inside, outside (0..1)
	const double Shore[][4] =
	{
		{    105.00,    140.00,      0.00,      0.00 },
		{    100.00,    130.00,      0.70,      0.70 },
		{    110.00,    120.00,      0.70,      0.70 },
		{    115.00,    110.00,      0.70,      0.90 },
		{    100.00,    115.00,      0.70,      1.00 },
		{     95.00,    110.00,      0.70,      0.90 },
		{    105.00,    120.00,      0.80,      0.70 },
		{    100.00,    100.00,      0.90,      0.70 },
		{     62.00,     64.00,      1.00,      0.80 },
		{     63.00,     62.00,      1.00,      0.90 },
		{    100.00,     95.00,      0.70,      0.70 },
		{    110.00,    105.00,      0.70,      0.90 },
		{    100.00,    110.00,      0.70,      1.00 },
		{     95.00,    100.00,      0.70,      0.80 },
		{     70.00,     72.00,      0.70,      0.70 },
		{     80.00,     95.00,      0.70,      0.70 },
	};

	const double BargeChannelMetres = 34.0;
	const double BargeOverrunMetres = 30.0;

	// Moving barges: from X, Y, to X, Y, round-trip seconds
	const double Barges[][5] =
	{
		{    671.39,   -208.26,    471.61,   -198.74,     44.00 },
		{   -618.15,   -195.47,   -425.85,   -140.53,     52.00 },
	};

}

namespace BoatTrackData
{
struct FLayout
{
    const double (&RacingLine)[16][2];
    const double (&Rocks)[9][3];
    const double (&Ramps)[4][3];
    const double (&Whirlpools)[3][3];
    const double (&Crates)[8][2];
    const double (&Debris)[12][2];
    const double (&Logs)[7][3];
    const double (&Shore)[16][4];
    const double (&Barges)[2][5];
};
const FLayout Layouts[] = {
    { BoatCourse0::RacingLine, BoatCourse0::Rocks, BoatCourse0::Ramps, BoatCourse0::Whirlpools, BoatCourse0::Crates, BoatCourse0::Debris, BoatCourse0::Logs, BoatCourse0::Shore, BoatCourse0::Barges },
    { BoatCourse1::RacingLine, BoatCourse1::Rocks, BoatCourse1::Ramps, BoatCourse1::Whirlpools, BoatCourse1::Crates, BoatCourse1::Debris, BoatCourse1::Logs, BoatCourse1::Shore, BoatCourse1::Barges },
    { BoatCourse2::RacingLine, BoatCourse2::Rocks, BoatCourse2::Ramps, BoatCourse2::Whirlpools, BoatCourse2::Crates, BoatCourse2::Debris, BoatCourse2::Logs, BoatCourse2::Shore, BoatCourse2::Barges },
};
const double GateHalfWidthMetres = 45.0;
const double BargeChannelMetres = 34.0;
const double BargeOverrunMetres = 30.0;
const FLayout& Current();
}
