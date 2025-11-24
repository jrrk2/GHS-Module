#include "ImageQualityEvaluator.h"
#include <pcl/FileFormat.h>
#include <pcl/FileFormatInstance.h>
#include <iostream>

using namespace pcl;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <image_file> [comparison_image_file]" << std::endl;
        return 1;
    }
    
    try
    {
        // Load the image
        FileFormat format(File::ExtractExtension(argv[1]), true, false);
        FileFormatInstance file(format);
        
        ImageDescriptionArray images;
        file.Open(images, argv[1]);
        
        if (images.IsEmpty())
        {
            std::cerr << "No images found in file: " << argv[1] << std::endl;
            return 1;
        }
        
        // Read the first image
        FImage image;
        file.ReadImage(image);
        file.Close();
        
        std::cerr << "Loaded image: " << argv[1] << std::endl;
        std::cerr << "Dimensions: " << image.Width() << "x" << image.Height() 
                  << " (" << image.NumberOfChannels() << " channels)" << std::endl;
        std::cerr << std::endl;
        
        // Evaluate the image
        ImageQualityEvaluator evaluator;
        ImageQualityMetrics metrics = evaluator.Evaluate(image);
        
        metrics.Print();
        
        // If a second image is provided, compare them
        if (argc >= 3)
        {
            FileFormat format2(File::ExtractExtension(argv[2]), true, false);
            FileFormatInstance file2(format2);
            
            ImageDescriptionArray images2;
            file2.Open(images2, argv[2]);
            
            if (!images2.IsEmpty())
            {
                FImage image2;
                file2.ReadImage(image2);
                file2.Close();
                
                std::cerr << "\n\nLoaded comparison image: " << argv[2] << std::endl;
                
                ImageQualityMetrics metrics2 = evaluator.Evaluate(image2);
                
                std::cerr << "\n=== Comparison Results ===" << std::endl;
                metrics2.Print();
                
                double improvement = metrics2.overallScore - metrics.overallScore;
                
                std::cerr << "\n=== Improvement Analysis ===" << std::endl;
                std::cerr << "Overall Score Change:   " << (improvement >= 0 ? "+" : "") << improvement << std::endl;
                std::cerr << "Star Sharpness Change:  " << (metrics2.starSharpness - metrics.starSharpness) << std::endl;
                std::cerr << "Background Smoothness:  " << (metrics2.backgroundSmoothness - metrics.backgroundSmoothness) << std::endl;
                std::cerr << "Dynamic Range Change:   " << (metrics2.dynamicRange - metrics.dynamicRange) << std::endl;
                std::cerr << "Contrast Change:        " << (metrics2.contrastScore - metrics.contrastScore) << std::endl;
                std::cerr << "Detail Preservation:    " << (metrics2.detailPreservation - metrics.detailPreservation) << std::endl;
                
                if (improvement > 5.0)
                    std::cerr << "\n✓ Second image shows significant improvement!" << std::endl;
                else if (improvement > 0.0)
                    std::cerr << "\n✓ Second image shows slight improvement" << std::endl;
                else if (improvement > -5.0)
                    std::cerr << "\n≈ Images are similar in quality" << std::endl;
                else
                    std::cerr << "\n✗ Second image shows degradation" << std::endl;
            }
        }
        
        return 0;
    }
    catch (const Exception& e)
    {
        std::cerr << "PCL Error: " << e.Message() << std::endl;
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
