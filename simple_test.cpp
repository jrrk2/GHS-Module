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
    
    static void SetIdentityParameters(pcl::GHSInstance& instance)
    {
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 0.0;
    }
    
    static void SetArcsinhParameters(pcl::GHSInstance& instance)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_Arcsinh;
        
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 2.0;
        
        double* SP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (SP) *SP = 0.2;
    }
    
    static void SetLinearParameters(pcl::GHSInstance& instance)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_Linear;
        
        double* BP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSBPParameter, 0));
        if (BP) *BP = 0.1;
        
        double* WP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSWPParameter, 0));
        if (WP) *WP = 0.9;
    }
    
    static void SetHyperbolicParameters(pcl::GHSInstance& instance)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_GeneralisedHyperbolic;
        
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 1.5;
        
        double* b = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSbParameter, 0));
        if (b) *b = 2.0;  // b > 0 for hyperbolic
        
        double* SP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (SP) *SP = 0.2;
    }
    
    static void SetMTFParameters(pcl::GHSInstance& instance)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_MidtonesTransfer;
        
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 1.0;
        
        double* SP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (SP) *SP = 0.25;
    }
    
    static void SetExponentialParameters(pcl::GHSInstance& instance)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_GeneralisedHyperbolic;
        
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 1.0;
        
        double* b = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSbParameter, 0));
        if (b) *b = 0.0;  // b = 0 for exponential
        
        double* SP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (SP) *SP = 0.15;
    }
    
    static void SetIntegralParameters(pcl::GHSInstance& instance)
    {
        pcl::pcl_enum* st = static_cast<pcl::pcl_enum*>(
            instance.LockParameter(pcl::TheGHSSTParameter, 0));
        if (st) *st = pcl::GHSST::ST_GeneralisedHyperbolic;
        
        double* D = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSParameter, 0));
        if (D) *D = 1.0;
        
        double* b = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSbParameter, 0));
        if (b) *b = -2.0;  // b < -1 for integral
        
        double* SP = static_cast<double*>(
            instance.LockParameter(pcl::TheGHSSPParameter, 0));
        if (SP) *SP = 0.1;
    }
};

int main(int argc, char *argv[])
{
    try {
        std::cout << "=== Starting GHS Test ===" << std::endl;
        
        // Set module handle
        void *dummy_handle = (void *)0xDEADBEEF;
        SetModuleHandle(dummy_handle);
        std::cout << "Module handle set" << std::endl;
        
        // Create module first (this should initialize the module system)
        std::cout << "Creating GHSModule..." << std::endl;
        pcl::GHSModule* module = new pcl::GHSModule();
        std::cout << "Module created: " << module->Name().c_str() << std::endl;
        
        // Create process (this initializes parameters)
        std::cout << "Creating GHSProcess..." << std::endl;
        pcl::GHSProcess* process = new pcl::GHSProcess();
        std::cout << "Process created: " << process->Id().c_str() << std::endl;
        
        // Verify parameters were initialized
        if (!pcl::TheGHSSTParameter) {
            std::cerr << "ERROR: TheGHSSTParameter not initialized!" << std::endl;
            return 1;
        }
        if (!pcl::TheGHSParameter) {
            std::cerr << "ERROR: TheGHSParameter not initialized!" << std::endl;
            return 1;
        }
        std::cout << "Parameters initialized successfully" << std::endl;
        
        // Create interface (optional for testing)
        std::cout << "Creating GHSInterface..." << std::endl;
        pcl::GHSInterface* interface = new pcl::GHSInterface();
        std::cout << "Interface created" << std::endl;
        
        // Test 1: Logarithmic transformation
        std::cout << "\n=== Test 1: Logarithmic GHS transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetParameters(sourceInstance, process);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            // Use public TransformationFlags() method
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  GHSLog: " << (flags.GHSLog ? "true" : "false") << std::endl;
            std::cout << "  GHSExp: " << (flags.GHSExp ? "true" : "false") << std::endl;
            std::cout << "  GHSHyp: " << (flags.GHSHyp ? "true" : "false") << std::endl;
            std::cout << "  GHSInt: " << (flags.GHSInt ? "true" : "false") << std::endl;
            std::cout << "  Linear: " << (flags.Linear ? "true" : "false") << std::endl;
            std::cout << "  Is invertible: " << (instance.IsInvertible() ? "Yes" : "No") << std::endl;
            
            std::cout << "\nGHS Logarithmic (D=1.0, b=-1.0, SP=0.1):" << std::endl;
            std::cout << "Input    -> Output" << std::endl;
            std::cout << "-------------------" << std::endl;
            
            for (double x = 0.0; x <= 1.0; x += 0.1) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        // Test 2: Identity transformation
        std::cout << "\n=== Test 2: Identity transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetParameters(sourceInstance, process);
            ParameterSetter::SetIdentityParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            std::cout << "Is identity: " << (instance.IsIdentityTransformation() ? "Yes" : "No") << std::endl;
            
            for (double x = 0.0; x <= 1.0; x += 0.25) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f (should be %.3f)\n", x, y, x);
            }
        }
        
        // Test 3: Arcsinh transformation
        std::cout << "\n=== Test 3: Arcsinh transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetArcsinhParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  GHSAsh: " << (flags.GHSAsh ? "true" : "false") << std::endl;
            
            std::cout << "\nArcsinh (D=2.0, SP=0.2):" << std::endl;
            for (double x = 0.0; x <= 1.0; x += 0.2) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        // Test 4: Linear transformation
        std::cout << "\n=== Test 4: Linear transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetLinearParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  Linear: " << (flags.Linear ? "true" : "false") << std::endl;
            
            std::cout << "\nLinear (BP=0.1, WP=0.9):" << std::endl;
            for (double x = 0.0; x <= 1.0; x += 0.1) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        // Test 5: Hyperbolic transformation
        std::cout << "\n=== Test 5: Hyperbolic transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetHyperbolicParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  GHSHyp: " << (flags.GHSHyp ? "true" : "false") << std::endl;
            
            std::cout << "\nGHS Hyperbolic (D=1.5, b=2.0, SP=0.2):" << std::endl;
            for (double x = 0.0; x <= 1.0; x += 0.1) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        // Test 6: MTF transformation
        std::cout << "\n=== Test 6: Midtones Transfer ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetMTFParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  GHSMtf: " << (flags.GHSMtf ? "true" : "false") << std::endl;
            
            std::cout << "\nMTF (D=1.0, SP=0.25):" << std::endl;
            for (double x = 0.0; x <= 1.0; x += 0.1) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        // Test 7: Exponential transformation
        std::cout << "\n=== Test 7: Exponential transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetExponentialParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  GHSExp: " << (flags.GHSExp ? "true" : "false") << std::endl;
            
            std::cout << "\nGHS Exponential (D=1.0, b=0.0, SP=0.15):" << std::endl;
            for (double x = 0.0; x <= 1.0; x += 0.1) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        // Test 8: Integral transformation
        std::cout << "\n=== Test 8: Integral transformation ===" << std::endl;
        {
            pcl::GHSInstance sourceInstance(process);
            ParameterSetter::SetIntegralParameters(sourceInstance);
            
            pcl::GHSInstance instance(process);
            instance.Assign(sourceInstance);
            
            pcl::GHSInstance::Flags flags = instance.TransformationFlags();
            std::cout << "Transformation flags:" << std::endl;
            std::cout << "  GHSInt: " << (flags.GHSInt ? "true" : "false") << std::endl;
            
            std::cout << "\nGHS Integral (D=1.0, b=-2.0, SP=0.1):" << std::endl;
            for (double x = 0.0; x <= 1.0; x += 0.1) {
                double y = x;
                instance.Transform(y);
                printf("%.3f    -> %.6f\n", x, y);
            }
        }
        
        std::cout << "\n=== All tests completed successfully ===" << std::endl;
        
        // Cleanup
        delete interface;
        delete process;
	//        delete module;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION caught" << std::endl;
        return 1;
    }
}
