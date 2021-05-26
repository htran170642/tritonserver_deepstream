/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "nvdsinfer_custom_impl.h"

#include <map>

#define kNMS_THRESH 0.45

static constexpr int LOCATIONS = 4;
struct alignas(float) Detection{
        //center_x center_y w h
        float bbox[LOCATIONS];
        float conf;  // bbox_conf * cls_conf
        float class_id;
    };

float iou(float lbox[4], float rbox[4]) {
    float interBox[] = {
        std::max(lbox[0] - lbox[2]/2.f , rbox[0] - rbox[2]/2.f), //left
        std::min(lbox[0] + lbox[2]/2.f , rbox[0] + rbox[2]/2.f), //right
        std::max(lbox[1] - lbox[3]/2.f , rbox[1] - rbox[3]/2.f), //top
        std::min(lbox[1] + lbox[3]/2.f , rbox[1] + rbox[3]/2.f), //bottom
    };

    if(interBox[2] > interBox[3] || interBox[0] > interBox[1])
        return 0.0f;

    float interBoxS =(interBox[1]-interBox[0])*(interBox[3]-interBox[2]);
    return interBoxS/(lbox[2]*lbox[3] + rbox[2]*rbox[3] -interBoxS);
}

bool cmp(Detection& a, Detection& b) {
    return a.conf > b.conf;
}

void nms(std::vector<Detection>& res, float *output, float conf_thresh, float nms_thresh,int num_bboxes,int num_classes) {
    int det_size = sizeof(Detection) / sizeof(float);
//     std::cout << sizeof(Detection) << " " << sizeof(float) <<std::endl;
//     std::cout << "detected before nms -> " << output[0] << std::endl;
    std::map<float, std::vector<Detection>> m;
    
    int conf_val = 4;
    for(int i=0; i< num_bboxes; i++) {
        if(output[conf_val] >= conf_thresh) {
            int idx = std::distance(output+conf_val+1, std::max_element(output+conf_val+1, output+ conf_val + num_classes-5+1));
            output[conf_val+1] = idx;
//             std::cout << output[conf_val-4] << " " << output[conf_val-3] << " " << output[conf_val-2] << " " << output[conf_val-1] << " "  << output[conf_val] << " " << idx <<std::endl;
//             std::cout << conf_val << " " << output[conf_val] << std::endl;
            
            Detection det;
            memcpy(&det, &output[conf_val-4], det_size * sizeof(float));
            if (m.count(det.class_id) == 0) m.emplace(det.class_id, std::vector<Detection>());
            m[det.class_id].push_back(det);
        }
        
        conf_val += num_classes;
    }
    
    for (auto it = m.begin(); it != m.end(); it++) {
        auto& dets = it->second;
        std::sort(dets.begin(), dets.end(), cmp);
        for (size_t m = 0; m < dets.size(); ++m) {
            auto& item = dets[m];
            res.push_back(item);
            for (size_t n = m + 1; n < dets.size(); ++n) {
                if (iou(item.bbox, dets[n].bbox) > nms_thresh) {
                    dets.erase(dets.begin()+n);
                    --n;
                }
            }
        }
    }
    
}

/* This is a sample bounding box parsing function for the sample YoloV5 detector model */
static bool NvDsInferParseYoloV5(
    std::vector<NvDsInferLayerInfo> const& outputLayersInfo,
    NvDsInferNetworkInfo const& networkInfo,
    NvDsInferParseDetectionParams const& detectionParams,
    std::vector<NvDsInferParseObjectInfo>& objectList)
{
    const float kCONF_THRESH = detectionParams.perClassThreshold[0];

    std::vector<Detection> res;

    const NvDsInferLayerInfo &output = outputLayersInfo[0]; // num_boxes x 4
    
    int num_bboxes = output.inferDims.d[0];
    int num_classes = output.inferDims.d[1];
    
//     std::cout << "Network Info: " << networkInfo.height << "  " << networkInfo.width << std::endl;
//     std::cout << num_bboxes << std::endl;
//     std::cout << num_classes << std::endl;
//     std::cout << output.inferDims.numDims << std::endl;
//     std::cout << boxes.inferDims.numElements << std::endl;
    
    nms(res, (float*)(outputLayersInfo[0].buffer), kCONF_THRESH, kNMS_THRESH, num_bboxes, num_classes);
//     std::cout << "after nms -> " << res.size() << std::endl;
    
    for(auto& r : res) {
	    NvDsInferParseObjectInfo oinfo;        
        
	    oinfo.classId = r.class_id;
	    oinfo.left    = static_cast<unsigned int>(r.bbox[0]-r.bbox[2]*0.5f);
	    oinfo.top     = static_cast<unsigned int>(r.bbox[1]-r.bbox[3]*0.5f);
	    oinfo.width   = static_cast<unsigned int>(r.bbox[2]);
	    oinfo.height  = static_cast<unsigned int>(r.bbox[3]);
//         std::cout << r.class_id << " " << oinfo.left << " "<<oinfo.top <<  " "<<oinfo.width <<  " "<<oinfo.height <<std::endl;
	    oinfo.detectionConfidence = r.conf;
	    objectList.push_back(oinfo);        
    }
    
    return true;
}

extern "C" bool NvDsInferParseCustomYoloV5(
    std::vector<NvDsInferLayerInfo> const &outputLayersInfo,
    NvDsInferNetworkInfo const &networkInfo,
    NvDsInferParseDetectionParams const &detectionParams,
    std::vector<NvDsInferParseObjectInfo> &objectList)
{
    return NvDsInferParseYoloV5(
        outputLayersInfo, networkInfo, detectionParams, objectList);
}

/* Check that the custom function has been defined correctly */
CHECK_CUSTOM_PARSE_FUNC_PROTOTYPE(NvDsInferParseCustomYoloV5);
