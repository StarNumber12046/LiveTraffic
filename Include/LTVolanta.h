/// @file       LTVolanta.h
/// @brief      Channel to Volanta traffic map
/// @see        https://fly.volanta.app/
/// @details    Defines SayIntentionsConnection:\n
///             Takes traffic from https://webassets.volanta.app/volanta-flight-positions.json
/// @author     StarNumber
/// @copyright  (c) 2025 StarNumber
/// @copyright  Permission is hereby granted, free of charge, to any person obtaining a
///             copy of this software and associated documentation files (the "Software"),
///             to deal in the Software without restriction, including without limitation
///             the rights to use, copy, modify, merge, publish, distribute, sublicense,
///             and/or sell copies of the Software, and to permit persons to whom the
///             Software is furnished to do so, subject to the following conditions:\n
///             The above copyright notice and this permission notice shall be included in
///             all copies or substantial portions of the Software.\n
///             THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
///             IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///             FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
///             AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
///             LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
///             OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
///             THE SOFTWARE.

#ifndef LTVolanta_h
#define LTVolanta_h

#include "LTChannel.h"

//
//MARK: Volanta Constants
//
#define VL_CHECK_NAME           "Volanta Flight Tracker"
#define VL_CHECK_URL            "https://fly.volanta.app"
#define VL_CHECK_POPUP          "See who's flying with Volanta just now"

#define VL_NAME                 "Volanta"
#define VL_URL_ALL              "https://webassets.volanta.app/volanta-flight-positions.json"

#define VL_DATA                 "data"

#define VL_NETWORK              "network"               ///< network (Vatsim, Volanta, IVAO)
#define VL_NETWORK_NAME         "Volanta"               ///< network name, e.g. "Volanta"

#define VL_KEY                  "id"
#define VL_POSITION             "position"              ///< position object
#define VL_LAT                  "latitude"
#define VL_LON                  "longitude"
#define VL_ALT                  "altitude"
#define VL_ALT_AGL              "altitudeAgl"
#define VL_DISPLAYNAME          "networkUserName"
#define VL_ORIGIN               "originIcao"
#define VL_DEST                 "destinationIcao"
#define VL_CALL                 "callsign"           ///< callsign machine-readable, e.g. "AAL2502"
#define VL_REG                  "aircraftRegistration"
#define VL_HEADING              "headingTrue"
#define VL_AC_TYPE              "aircraftIcao"
#define VL_SPD                  "groundSpeed"

//
// MARK: Volanta connection class
//

/// Connection to Volanta
class VolantaConnection : public LTFlightDataChannel
{
protected:
    double  tsRequest = NAN;                                ///< when did we send the last request?
public:
    VolantaConnection ();                             ///< Constructor
    std::string GetURL (const positionTy& pos) override;    ///< returns the constant URL to SayIntentions traffic
    bool ProcessFetchedData () override;                    ///< Process response, selecting traffic around us and forwarding to the processing queues
protected:
    void Main () override;                                  ///< virtual thread main function
};

#endif /* LTSayIntentions_h */
