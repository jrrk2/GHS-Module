#include "Assert.h"
#include "GHSInstance.h"
#include "GHSInterface.h"
#include "GHSModule.h"
#include "GHSParameters.h"
#include "GHSProcess.h"
#include "PCLMockAPI.h"
#include "PCLThreadMock.h"
#include "RgbPreserve.h"
#include "solver_dct3.h"

#include <pcl/FileFormat.h>
#include <pcl/FileFormatInstance.h>
#include <pcl/Console.h>
#include <pcl/File.h>

#include <iostream>
#include <memory>

extern "C" void SetModuleHandle(void *);

// Helper class to set parameters through LockParameter interface
class ParameterSetter
{
public:
    static void SetParameters(pcl::GHSInstance& instance, pcl::GHSProcess* process)
    {
        // Lock and set each parameter using the proper interface
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_GeneralisedHyperbolic;
        
        pcl::pcl_enum* sc = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSCParameter, 0));
        if (sc) *sc = pcl::GHSSC::SC_RGB;
        
        pcl::pcl_bool* inv = static_cast<pcl::pcl_bool*>(
            instance.LockParameter(pcl::TheGHSInvParameter, 0));
        if (inv) *inv = false;
        
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 1.0;
        
        double* b = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSbParameter, 0));
        if (b) *b = -1.0;  // Logarithmic
        
        double* SP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (SP) *SP = 0.1;
        
        double* LP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSLPParameter, 0));
        if (LP) *LP = 0.0;
        
        double* HP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSHPParameter, 0));
        if (HP) *HP = 1.0;
        
        double* BP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSBPParameter, 0));
        if (BP) *BP = 0.0;
        
        double* WP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSWPParameter, 0));
        if (WP) *WP = 1.0;
        
        double* CB = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSCBParameter, 0));
        if (CB) *CB = 1.0;
        
        pcl::pcl_enum* ct = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSCTParameter, 0));
        if (ct) *ct = pcl::GHSCT::CT_Clip;
        
        pcl::pcl_bool* rgbws = static_cast<pcl::pcl_bool*>(
            instance.LockParameter(pcl::TheGHSRGBWSParameter, 0));
        if (rgbws) *rgbws = false;
    }
    
    static void SetCustomParameters(pcl::GHSInstance& instance, 
                                    int stretchType, double D, double b, double SP)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = stretchType;
        
        pcl::pcl_enum* sc = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSCParameter, 0));
        if (sc) *sc = pcl::GHSSC::SC_RGB;
        
        pcl::pcl_bool* inv = static_cast<pcl::pcl_bool*>(
            instance.LockParameter(pcl::TheGHSInvParameter, 0));
        if (inv) *inv = false;
        
        double* pD = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (pD) *pD = D;
        
        double* pb = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSbParameter, 0));
        if (pb) *pb = b;
        
        double* pSP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (pSP) *pSP = SP;
        
        double* LP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSLPParameter, 0));
        if (LP) *LP = 0.0;
        
        double* HP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSHPParameter, 0));
        if (HP) *HP = 1.0;
        
        double* BP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSBPParameter, 0));
        if (BP) *BP = 0.0;
        
        double* WP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSWPParameter, 0));
        if (WP) *WP = 1.0;
        
        double* CB = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSCBParameter, 0));
        if (CB) *CB = 1.0;
        
        pcl::pcl_enum* ct = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSCTParameter, 0));
        if (ct) *ct = pcl::GHSCT::CT_Clip;
        
        pcl::pcl_bool* rgbws = static_cast<pcl::pcl_bool*>(
            instance.LockParameter(pcl::TheGHSRGBWSParameter, 0));
        if (rgbws) *rgbws = false;
    }
};

bool ProcessImage(const pcl::String& inputPath, const pcl::String& outputPath,
                 int stretchType, double D, double b, double SP)
{
    try {
        std::cout << "\n=== Processing Image ===" << std::endl;
        std::cout << "Input:  " << inputPath.c_str() << std::endl;
        std::cout << "Output: " << outputPath.c_str() << std::endl;
        
        // Extract file extension and find format
        pcl::String ext = pcl::File::ExtractExtension(inputPath);
        std::cout << "Detected file extension: " << ext.c_str() << std::endl;
        
        // Create FileFormat for reading (using file extension)
        std::cout << "Looking for file format..." << std::endl;
        pcl::FileFormat format(ext, true, false);  // toRead=true, toWrite=false
        
        std::cout << "Found format: " << format.Name().c_str() << std::endl;
        
        // Create a format instance
        std::cout << "Creating format instance..." << std::endl;
        pcl::FileFormatInstance file(format);
        
        // Open the input file
        std::cout << "Opening input file..." << std::endl;
        pcl::ImageDescriptionArray images;
        if (!file.Open(images, inputPath))
        {
            std::cerr << "ERROR: Unable to open input file" << std::endl;
            return false;
        }
        
        if (images.IsEmpty())
        {
            std::cerr << "ERROR: No images found in input file" << std::endl;
            file.Close();
            return false;
        }
        
        std::cout << "Image info:" << std::endl;
        std::cout << "  Number of images: " << images.Length() << std::endl;
        std::cout << "  Width:    " << images[0].info.width << std::endl;
        std::cout << "  Height:   " << images[0].info.height << std::endl;
        std::cout << "  Channels: " << images[0].info.numberOfChannels << std::endl;
        
        pcl::String colorSpaceName;
        switch (images[0].info.colorSpace)
        {
            case pcl::ColorSpace::Gray: colorSpaceName = "Grayscale"; break;
            case pcl::ColorSpace::RGB:  colorSpaceName = "RGB"; break;
            default: colorSpaceName = "Unknown"; break;
        }
        std::cout << "  Color:    " << colorSpaceName.c_str() << std::endl;
        
        // Select the first image
        if (!file.SelectImage(0))
        {
            std::cerr << "ERROR: Unable to select image" << std::endl;
            file.Close();
            return false;
        }
        
        // Read the image into a 32-bit floating point image
        std::cout << "Reading image data..." << std::endl;
        pcl::Image image;
        if (!file.ReadImage(image))
        {
            std::cerr << "ERROR: Unable to read image data" << std::endl;
            file.Close();
            return false;
        }
        
        file.Close();
        
        std::cout << "Image loaded successfully" << std::endl;
        std::cout << "  Actual dimensions: " << image.Width() << "x" << image.Height() << std::endl;
        std::cout << "  Actual channels: " << image.NumberOfChannels() << std::endl;
        
        // Create the GHS instance and set parameters
        std::cout << "\nSetting up GHS transformation..." << std::endl;
        pcl::GHSProcess* process = pcl::TheGHSProcess;
        pcl::GHSInstance sourceInstance(process);
        ParameterSetter::SetCustomParameters(sourceInstance, stretchType, D, b, SP);
        
        pcl::GHSInstance instance(process);
        instance.Assign(sourceInstance);
        
        // Display transformation info
        pcl::GHSInstance::Flags flags = instance.TransformationFlags();
        std::cout << "Transformation type: ";
        if (flags.GHSLog) std::cout << "Logarithmic" << std::endl;
        else if (flags.GHSExp) std::cout << "Exponential" << std::endl;
        else if (flags.GHSHyp) std::cout << "Hyperbolic" << std::endl;
        else if (flags.GHSInt) std::cout << "Integral" << std::endl;
        else if (flags.GHSMtf) std::cout << "Midtones Transfer" << std::endl;
        else if (flags.GHSAsh) std::cout << "Arcsinh" << std::endl;
        else if (flags.Linear) std::cout << "Linear" << std::endl;
        else std::cout << "Unknown" << std::endl;
        
        std::cout << "Parameters: D=" << D << ", b=" << b << ", SP=" << SP << std::endl;
        
        // Execute the transformation
        std::cout << "Applying GHS transformation..." << std::endl;
        pcl::ImageVariant imageVariant(&image);
        
        if (!instance.ExecuteOn(imageVariant, ""))
        {
            std::cerr << "ERROR: Transformation failed" << std::endl;
            return false;
        }
        
        std::cout << "Transformation completed successfully" << std::endl;
        
        // Save the result
        std::cout << "\nSaving output file..." << std::endl;
        pcl::String outExt = pcl::File::ExtractExtension(outputPath);
        std::cout << "Output format: " << outExt.c_str() << std::endl;
        
        // Create FileFormat for writing
        pcl::FileFormat outputFormat(outExt, false, true);  // toRead=false, toWrite=true
        std::cout << "Using output format: " << outputFormat.Name().c_str() << std::endl;
        
        pcl::FileFormatInstance outputFile(outputFormat);
        if (!outputFile.Create(outputPath))
        {
            std::cerr << "ERROR: Unable to create output file" << std::endl;
            return false;
        }
        
        if (!outputFile.WriteImage(image))
        {
            std::cerr << "ERROR: Unable to write image data" << std::endl;
            outputFile.Close();
            return false;
        }
        
        outputFile.Close();
        
        std::cout << "Image saved successfully!" << std::endl;
        return true;
        
    } catch (const pcl::Exception& e) {
        std::cerr << "PCL EXCEPTION during image processing: " << e.Message().c_str() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "STD EXCEPTION during image processing: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION during image processing" << std::endl;
        return false;
    }
}

void PrintUsage()
{
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  Program [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --test              Run transformation tests (default)" << std::endl;
    std::cout << "  --image <input> <output> [options]" << std::endl;
    std::cout << "                      Process an image file" << std::endl;
    std::cout << "  --stretch <type>    Stretch type: 0=GHS, 1=MTF, 2=Arcsinh, 3=Linear (default: 0)" << std::endl;
    std::cout << "  --D <value>         Stretch factor (default: 1.0)" << std::endl;
    std::cout << "  --b <value>         Local intensity (default: -1.0)" << std::endl;
    std::cout << "  --SP <value>        Symmetry point (default: 0.1)" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  Program --test" << std::endl;
    std::cout << "  Program --image input.fits output.fits" << std::endl;
    std::cout << "  Program --image input.xisf output.xisf --D 2.0 --b 0.0 --SP 0.2" << std::endl;
    std::cout << "  Program --image input.tif output.tif --stretch 2 --D 1.5" << std::endl;
}

int main(int argc, char *argv[])
{
    try {
        std::cout << "=== GHS Image Processing Tool ===" << std::endl;
        
        // Set module handle
        void *dummy_handle = (void *)0xDEADBEEF;
        SetModuleHandle(dummy_handle);
        
        // Initialize PCL modules
        pcl::GHSModule* module = new pcl::GHSModule();
        pcl::GHSProcess* process = new pcl::GHSProcess();
        pcl::GHSInterface* interface = new pcl::GHSInterface();
        
        // Verify parameters were initialized
        if (!pcl::TheGHSSTParameter || !pcl::TheGHSParameter) {
            std::cerr << "ERROR: Parameters not initialized!" << std::endl;
            return 1;
        }
        
        std::cout << "Modules initialized successfully" << std::endl;
        
        // Parse command line arguments
        bool runTests = true;
        bool processImageFlag = false;
        pcl::String inputPath, outputPath;
        int stretchType = pcl::GHSST::ST_GeneralisedHyperbolic;
        double D = 1.0, b = -1.0, SP = 0.1;
        
        for (int i = 1; i < argc; ++i)
        {
            pcl::String arg(argv[i]);
            
            if (arg == "--test")
            {
                runTests = true;
                processImageFlag = false;
            }
            else if (arg == "--image")
            {
                if (i + 2 >= argc)
                {
                    std::cerr << "ERROR: --image requires input and output paths" << std::endl;
                    PrintUsage();
                    return 1;
                }
                inputPath = argv[++i];
                outputPath = argv[++i];
                processImageFlag = true;
                runTests = false;
            }
            else if (arg == "--stretch")
            {
                if (++i >= argc)
                {
                    std::cerr << "ERROR: --stretch requires a value" << std::endl;
                    return 1;
                }
                stretchType = pcl::String(argv[i]).ToInt();
            }
            else if (arg == "--D")
            {
                if (++i >= argc)
                {
                    std::cerr << "ERROR: --D requires a value" << std::endl;
                    return 1;
                }
                D = pcl::String(argv[i]).ToDouble();
            }
            else if (arg == "--b")
            {
                if (++i >= argc)
                {
                    std::cerr << "ERROR: --b requires a value" << std::endl;
                    return 1;
                }
                b = pcl::String(argv[i]).ToDouble();
            }
            else if (arg == "--SP")
            {
                if (++i >= argc)
                {
                    std::cerr << "ERROR: --SP requires a value" << std::endl;
                    return 1;
                }
                SP = pcl::String(argv[i]).ToDouble();
            }
            else if (arg == "--help" || arg == "-h")
            {
                PrintUsage();
                return 0;
            }
            else
            {
                std::cerr << "ERROR: Unknown option: " << arg.c_str() << std::endl;
                PrintUsage();
                return 1;
            }
        }
        
        // Run tests if requested
        if (runTests)
        {
            std::cout << "\n=== Running Transformation Tests ===" << std::endl;
            
            // Test 1: Logarithmic
            std::cout << "\nTest 1: Logarithmic (D=1.0, b=-1.0, SP=0.1)" << std::endl;
            {
                pcl::GHSInstance sourceInstance(process);
                ParameterSetter::SetParameters(sourceInstance, process);
                pcl::GHSInstance instance(process);
                instance.Assign(sourceInstance);
                
                for (double x = 0.0; x <= 1.0; x += 0.2) {
                    double y = x;
                    instance.Transform(y);
                    printf("  %.3f -> %.6f\n", x, y);
                }
            }
            
            std::cout << "\n=== Tests completed successfully ===" << std::endl;
        }
        
        // Process image if requested
        if (processImageFlag)
        {
            bool success = ProcessImage(inputPath, outputPath, stretchType, D, b, SP);
            
            // Cleanup
            delete interface;
            delete process;
            delete module;
            
            return success ? 0 : 1;
        }
        
        // Cleanup
        delete interface;
        delete process;
        // delete module;
        
        return 0;
        
    } catch (const pcl::Exception& e) {
        std::cerr << "PCL EXCEPTION: " << e.Message().c_str() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "STD EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION caught" << std::endl;
        return 1;
    }
}
