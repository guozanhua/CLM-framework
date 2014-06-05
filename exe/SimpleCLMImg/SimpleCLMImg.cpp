///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012, Tadas Baltrusaitis, all rights reserved.
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
//     * The software is provided under the terms of this licence stricly for
//       academic, non-commercial, not-for-profit purposes.
//     * Redistributions of source code must retain the above copyright notice, 
//       this list of conditions (licence) and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright 
//       notice, this list of conditions (licence) and the following disclaimer 
//       in the documentation and/or other materials provided with the 
//       distribution.
//     * The name of the author may not be used to endorse or promote products 
//       derived from this software without specific prior written permission.
//     * As this software depends on other libraries, the user must adhere to 
//       and keep in place any licencing terms of those libraries.
//     * Any publications arising from the use of this software, including but
//       not limited to academic journal and conference publications, technical
//       reports and manuals, must cite one of the following works:
//
//       Tadas Baltrusaitis, Peter Robinson, and Louis-Philippe Morency. 3D
//       Constrained Local Model for Rigid and Non-Rigid Facial Tracking.
//       IEEE Conference on Computer Vision and Pattern Recognition (CVPR), 2012.    
//
//       Tadas Baltrusaitis, Peter Robinson, and Louis-Philippe Morency. 
//       Constrained Local Neural Fields for robust facial landmark detection in the wild.
//       in IEEE Int. Conference on Computer Vision Workshops, 300 Faces in-the-Wild Challenge, 2013.    
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED 
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO 
// EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include <CLM.h>
#include <CLMTracker.h>
#include <CLMParameters.h>

#include <fstream>

#include <cxcore.h>
#include <highgui.h>

#include <filesystem.hpp>
#include <filesystem/fstream.hpp>

using namespace std;
using namespace cv;

vector<string> get_arguments(int argc, char **argv)
{

	vector<string> arguments;

	for(int i = 1; i < argc; ++i)
	{
		arguments.push_back(string(argv[i]));
	}
	return arguments;
}

void convert_to_grayscale(const Mat& in, Mat& out)
{
	if(in.channels() == 3)
	{
		// Make sure it's in a correct format
		if(in.depth() != CV_8U)
		{
			if(in.depth() == CV_16U)
			{
				Mat tmp = in / 256;
				tmp.convertTo(tmp, CV_8U);
				cvtColor(tmp, out, CV_BGR2GRAY);
			}
		}
		else
		{
			cvtColor(in, out, CV_BGR2GRAY);
		}
	}
	else
	{
		if(in.depth() == CV_16U)
		{
			Mat tmp = in / 256;
			out = tmp.clone();
		}
		else
		{
			out = in.clone();
		}
	}
}

void write_out_landmarks(const string& outfeatures, const CLMTracker::CLM& clm_model)
{

	std::ofstream featuresFile;
	featuresFile.open(outfeatures);		

	if(featuresFile.is_open())
	{	
		int n = clm_model.patch_experts.visibilities[0][0].rows;
		featuresFile << "version: 1" << endl;
		featuresFile << "npoints: " << n << endl;
		featuresFile << "{" << endl;
		
		for (int i = 0; i < n; ++ i)
		{
			// Use matlab format, so + 1
			featuresFile << clm_model.detected_landmarks.at<double>(i) + 2 << " " << clm_model.detected_landmarks.at<double>(i+n) + 2 << endl;
		}
		featuresFile << "}" << endl;			
		featuresFile.close();
	}
}

void create_display_image(const Mat& orig, Mat& display_image, CLMTracker::CLM& clm_model)
{
	
	// preparing the visualisation image
	display_image = orig.clone();		

	// Creating a display image			
	Mat xs = clm_model.detected_landmarks(Rect(0, 0, 1, clm_model.detected_landmarks.rows/2));
	Mat ys = clm_model.detected_landmarks(Rect(0, clm_model.detected_landmarks.rows/2, 1, clm_model.detected_landmarks.rows/2));
	double min_x, max_x, min_y, max_y;

	cv::minMaxLoc(xs, &min_x, &max_x);
	cv::minMaxLoc(ys, &min_y, &max_y);

	double width = max_x - min_x;
	double height = max_y - min_y;

	int minCropX = max((int)(min_x-width/3.0),0);
	int minCropY = max((int)(min_y-height/3.0),0);

	int widthCrop = min((int)(width*5.0/3.0), display_image.cols - minCropX - 1);
	int heightCrop = min((int)(height*5.0/3.0), display_image.rows - minCropY - 1);

	double scaling = 350.0/widthCrop;
	
	// first crop the image
	display_image = display_image(Rect((int)(minCropX), (int)(minCropY), (int)(widthCrop), (int)(heightCrop)));
		
	// now scale it
	cv::resize(display_image.clone(), display_image, Size(), scaling, scaling);

	// Make the adjustments to points
	xs = (xs - minCropX)*scaling;
	ys = (ys - minCropY)*scaling;

	Mat shape = clm_model.detected_landmarks.clone();

	xs.copyTo(shape(Rect(0, 0, 1, clm_model.detected_landmarks.rows/2)));
	ys.copyTo(shape(Rect(0, clm_model.detected_landmarks.rows/2, 1, clm_model.detected_landmarks.rows/2)));

	CLMTracker::Draw(display_image, clm_model);
						
}

int main (int argc, char **argv)
{

	//Convert arguments to more convenient vector form
	vector<string> arguments = get_arguments(argc, argv);

	// Some initial parameters that can be overriden from command line
	vector<string> files, depth_files, output_images, output_landmark_locations;

	// Bounding boxes for a face in each image (optional)
	vector<Rect_<double> > bounding_boxes;
	
	CLMTracker::get_image_input_output_params(files, depth_files, output_landmark_locations, output_images, bounding_boxes, arguments);	
	CLMTracker::CLMParameters clm_parameters(arguments);	
	// No need to validate detections, as we're not doing tracking
	clm_parameters.validate_detections = false;

	// The modules that are being used for tracking
	cout << "Loading the model" << endl;
	CLMTracker::CLM clm_model(clm_parameters.model_location);
	cout << "Model loaded" << endl;

	CascadeClassifier classifier(clm_parameters.face_detector_location);

	bool visualise = !clm_parameters.quiet_mode;

	// Do some image loading
	for(size_t i = 0; i < files.size(); i++)
	{
		string file = files.at(i);

		// Loading image
		Mat read_image = imread(file, -1);
		
		// Loading depth file if exists (optional)
		Mat_<float> depth_image;

		if(depth_files.size() > 0)
		{
			string dFile = depth_files.at(i);
			Mat dTemp = imread(dFile, -1);
			dTemp.convertTo(depth_image, CV_32F);
		}
		
		// Making sure the image is in uchar grayscale
		Mat_<uchar> grayscale_image;		
		convert_to_grayscale(read_image, grayscale_image);
					
		// if no pose defined we just use OpenCV
		if(bounding_boxes.empty())
		{
			
			// Detect faces in an image
			vector<Rect_<double> > face_detections;

			CLMTracker::DetectFaces(face_detections, grayscale_image, classifier);

			// Detect landmarks around detected faces

			// perform landmark detection for every face detected
			for(size_t face=0; face < face_detections.size(); ++face)
			{
				// if there are multiple detections go through them
				CLMTracker::DetectLandmarksInImage(grayscale_image, depth_image, face_detections[face], clm_model, clm_parameters);

				// Writing out the detected landmarks (in an OS independent manner)
				if(!output_landmark_locations.empty())
				{
					char name[100];
					// append detection number (in case multiple faces are detected)
					sprintf(name, "_det_%d", face);

					// Construct the output filename
					boost::filesystem::path slash("/");
					std::string preferredSlash = slash.make_preferred().string();

					boost::filesystem::path out_feat_path(output_landmark_locations.at(i));
					boost::filesystem::path dir = out_feat_path.parent_path();
					boost::filesystem::path fname = out_feat_path.filename().replace_extension("");
					boost::filesystem::path ext = out_feat_path.extension();
					string outfeatures = dir.string() + preferredSlash + fname.string() + string(name) + ext.string();
					write_out_landmarks(outfeatures, clm_model);
				}

				// displaying detected landmarks
				Mat display_image;
				create_display_image(read_image, display_image, clm_model);

				if(visualise)
				{
					imshow("colour", display_image);
					cv::waitKey(1);
				}

				// Saving the display images (in an OS independent manner)
				if(!output_images.empty())
				{
					string outimage = output_images.at(i);
					if(!outimage.empty())
					{
						char name[100];
						sprintf(name, "_det_%d", face);

						boost::filesystem::path slash("/");
						std::string preferredSlash = slash.make_preferred().string();

						// append detection number
						boost::filesystem::path out_feat_path(outimage);
						boost::filesystem::path dir = out_feat_path.parent_path();
						boost::filesystem::path fname = out_feat_path.filename().replace_extension("");
						boost::filesystem::path ext = out_feat_path.extension();
						outimage = dir.string() + preferredSlash + fname.string() + string(name) + ext.string();

						imwrite(outimage, display_image);	
					}
				}


			}
		}
		else
		{
			// Have provided bounding boxes
			CLMTracker::DetectLandmarksInImage(grayscale_image, bounding_boxes[i], clm_model, clm_parameters);

			// Writing out the detected landmarks
			if(!output_landmark_locations.empty())
			{
				string outfeatures = output_landmark_locations.at(i);
				write_out_landmarks(outfeatures, clm_model);
			}

			// displaying detected stuff
			Mat display_image;
			create_display_image(read_image, display_image, clm_model);

			if(visualise)
			{
				imshow("colour", display_image);
				cv::waitKey(1);
			}

			if(!output_images.empty())
			{
				string outimage = output_images.at(i);
				if(!outimage.empty())
				{
					imwrite(outimage, display_image);	
				}
			}
		}				

	}
	return 0;
}

