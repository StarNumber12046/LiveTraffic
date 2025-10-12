/// @file       LTVolanta.cpp
/// @brief      Channel to Volanta traffic map
/// @see        https://fly.volanta.app/
/// @details    Defines VolantaConnection:\n
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

#include "LiveTraffic.h"

//
// MARK: Volanta
//

// Constructor
VolantaConnection::VolantaConnection () :
LTFlightDataChannel(DR_CHANNEL_VOLANTA, VL_NAME)
{
    // purely informational
    urlName  = VL_CHECK_NAME;
    urlLink  = VL_CHECK_URL;
    urlPopup = VL_CHECK_POPUP;
}

// virtual thread main function
void VolantaConnection::Main ()
{
    // This is a communication thread's main function, set thread's name and C locale
    ThreadSettings TS ("VL_SI", LC_ALL_MASK);
    
    while ( shallRun() ) {
        // LiveTraffic Top Level Exception Handling
        try {
            // basis for determining when to be called next
            tNextWakeup = std::chrono::steady_clock::now();
            
            // where are we right now?
            const positionTy pos (dataRefs.GetViewPos());
            
            // If the camera position is valid we can request data around it
            if (pos.isNormal()) {
                // Next wakeup is "refresh interval" from _now_
                tNextWakeup += std::chrono::seconds(dataRefs.GetFdRefreshIntvl());
                
                // fetch data and process it
                tsRequest = dataRefs.GetSimTime() + dataRefs.GetFdBufPeriod();
                if (FetchAllData(pos) && ProcessFetchedData())
                        // reduce error count if processed successfully
                        // as a chance to appear OK in the long run
                        DecErrCnt();
            }
            else {
                // Camera position is yet invalid, retry in a second
                tNextWakeup += std::chrono::seconds(1);
            }
            
            // sleep for FD_REFRESH_INTVL or if woken up for termination
            // by condition variable trigger
            {
                std::unique_lock<std::mutex> lk(FDThreadSynchMutex);
                FDThreadSynchCV.wait_until(lk, tNextWakeup,
                                           [this]{return !shallRun();});
            }
            
        } catch (const std::exception& e) {
            LOG_MSG(logERR, ERR_TOP_LEVEL_EXCEPTION, e.what());
            IncErrCnt();
        } catch (...) {
            LOG_MSG(logERR, ERR_TOP_LEVEL_EXCEPTION, "(unknown type)");
            IncErrCnt();
        }
    }
}


// returns the constant URL to SayIntentions traffic
std::string VolantaConnection::GetURL (const positionTy&)
{
    return VL_URL_ALL;
}

bool VolantaConnection::ProcessFetchedData()
{
    LOG_MSG(logDEBUG, "[Volanta] Processing fetched data...");

    if (!netDataPos) {
        LOG_MSG(logDEBUG, "[Volanta] No network data to process (netDataPos is null).");
        return true;
    }

    if (httpResponse != HTTP_OK) {
        LOG_MSG(logWARN, "[Volanta] HTTP response not OK: %d", httpResponse);
        IncErrCnt();
        return false;
    }

    LOG_MSG(logDEBUG, "[Volanta] Parsing JSON data (%zu bytes)...", strlen(netData));

    JSONRootPtr pRoot(netData);
    if (!pRoot) {
        LOG_MSG(logERR, "[Volanta] Failed to parse JSON: %s", ERR_JSON_PARSE);
        IncErrCnt();
        return false;
    }

    const JSON_Object* pObjRoot = json_value_get_object(pRoot.get());
    if (!pObjRoot) {
        LOG_MSG(logERR, "[Volanta] JSON root is not an object (type %d)",
            (int)json_type(pRoot.get()));
        return false;
    }

    LOG_MSG(logDEBUG, "[Volanta] Accessing array field: %s", VL_DATA);
    const JSON_Array* pArrAc = json_object_dotget_array(pObjRoot, VL_DATA);
    if (!pArrAc) {
        LOG_MSG(logERR, "[Volanta] Expected JSON array in field '%s' (type %d)",
            VL_DATA, (int)json_type(pRoot.get()));
        IncErrCnt();
        return false;
    }

    size_t aircraftCount = json_array_get_count(pArrAc);
    LOG_MSG(logDEBUG, "[Volanta] Found %zu aircraft entries.", aircraftCount);

    const positionTy viewPos = dataRefs.GetViewPos();
    const std::string acFilter(dataRefs.GetDebugAcFilter());

    size_t processedCount = 0, skippedCount = 0, invalidCount = 0;

    for (size_t i = 0; i < aircraftCount; ++i) {
        const JSON_Object* aircraftJson = json_array_get_object(pArrAc, i);
        if (!aircraftJson) {
            LOG_MSG(logDEBUG, "[Volanta] Skipping null aircraft at index %zu.", i);
            invalidCount++;
            continue;
        }

        LOG_MSG(logDEBUG, "[Volanta] === Aircraft #%zu ===", i);

        const std::string network = jog_s(aircraftJson, VL_NETWORK);
        LOG_MSG(logDEBUG, "[Volanta] network = '%s'", network.c_str());
        if (network != VL_NETWORK_NAME) {
            LOG_MSG(logDEBUG, "[Volanta] Skipping non-%s aircraft (network=%s).",
                VL_NETWORK_NAME, network.c_str());
            skippedCount++;
            continue;
        }

        const std::string displayName = jog_s(aircraftJson, VL_DISPLAYNAME);
        LOG_MSG(logDEBUG, "[Volanta] displayName = '%s'", displayName.c_str());
		LOG_MSG(logDEBUG, "[Volanta] Own a/c display name is '%s'", dataRefs.GetSIDisplayName());
        // Displayname is matching? My own flight! -> Skip it
        if (displayName == dataRefs.GetSIDisplayName()) // Still says SI, I dont care, will fix maybe
            continue;
        LTFlightData::FDKeyTy fdKey(LTFlightData::KEY_VOLANTA, jog_s(aircraftJson, VL_KEY));

        if (!acFilter.empty() && (fdKey != acFilter)) {
            LOG_MSG(logDEBUG, "[Volanta] Skipping a/c '%s' not matching filter '%s'.",
                fdKey.c_str(), acFilter.c_str());
            skippedCount++;
            continue;
        }

        const JSON_Object* posJson = json_object_dotget_object(aircraftJson, VL_POSITION);
        if (!posJson) {
            LOG_MSG(logWARN, "[Volanta] Missing 'position' field for '%s'.", fdKey.c_str());
            invalidCount++;
            continue;
        }

        double lat = jog_n_nan(posJson, VL_LAT);
        double lon = jog_n_nan(posJson, VL_LON);
        double alt = jog_l(posJson, VL_ALT);
        double heading = jog_l(posJson, VL_HEADING);

        positionTy pos(lat, lon, alt, tsRequest, heading);
        double dist = pos.dist(viewPos);

        if (dist > dataRefs.GetFdStdDistance_m()) {
            LOG_MSG(logDEBUG, "[Volanta] Skipping '%s' (distance %.1fm exceeds limit %.1fm).",
                fdKey.c_str(), dist, dataRefs.GetFdStdDistance_m());
            skippedCount++;
            continue;
        }

        double altAgl = jog_l(aircraftJson, VL_ALT_AGL);
        if (altAgl <= 0)
            pos.f.onGrnd = GND_ON;

        std::unique_lock<std::mutex> mapFdLock(mapFdMutex);
        LTFlightData& fd = mapFd[fdKey];
        std::lock_guard<std::recursive_mutex> fdLock(fd.dataAccessMutex);
        mapFdLock.unlock();

        if (fd.empty()) fd.SetKey(fdKey);

        LTFlightData::FDStaticData stat;
        stat.reg = jog_s(aircraftJson, VL_REG);
        stat.acTypeIcao = jog_s(aircraftJson, VL_AC_TYPE);
        stat.call = jog_s(aircraftJson, VL_CALL);
        if (stat.call.empty()) stat.call = "Unknown";
        stat.setOrigDest(jog_s(aircraftJson, VL_ORIGIN),
            jog_s(aircraftJson, VL_DEST));
        stat.flight = displayName;

        LTFlightData::FDDynamicData dyn;
        dyn.gnd = pos.IsOnGnd();
        dyn.heading = pos.heading();
        dyn.spd = jog_n_nan(aircraftJson, VL_SPD);
        dyn.ts = pos.ts();
        dyn.pChannel = this;

        fd.UpdateData(std::move(stat), dist);

        if (pos.isNormal(true)) {
            fd.AddDynData(dyn, 0, 0, &pos);
            processedCount++;
        }
        else {
            LOG_MSG(logDEBUG, "[Volanta] Ignoring abnormal position for '%s' (%s).",
                fdKey.c_str(), pos.dbgTxt().c_str());
            invalidCount++;
        }
    }

    LOG_MSG(logINFO, "[Volanta] Processed=%zu, Skipped=%zu, Invalid=%zu (total=%zu)",
        processedCount, skippedCount, invalidCount, aircraftCount);

    return true;
}
