#ifndef __ImageQualityEvaluator_h
#define __ImageQualityEvaluator_h

#include <pcl/Image.h>
#include <pcl/Matrix.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>

namespace pcl
{

// Structure to hold evaluation metrics
struct ImageQualityMetrics
{
    // Overall score (0-100, higher is better)
    double overallScore = 0.0;
    
    // Individual metrics
    double starSharpness = 0.0;      // 0-100: How sharp the stars are
    double backgroundSmoothness = 0.0; // 0-100: How smooth/noise-free the background is
    double dynamicRange = 0.0;        // 0-100: Utilization of available dynamic range
    double contrastScore = 0.0;       // 0-100: Overall contrast quality
    double detailPreservation = 0.0;  // 0-100: Amount of fine detail preserved
    double clippingPenalty = 0.0;     // 0-100: Penalty for clipped highlights/shadows
    
    // Statistical data
    double meanValue = 0.0;
    double stdDev = 0.0;
    double medianValue = 0.0;
    double snr = 0.0;                 // Signal-to-noise ratio
    
    // Histogram analysis
    double histogramPeak = 0.0;       // Where most pixels are concentrated
    double histogramSpread = 0.0;     // How spread out the histogram is
    
    void Print() const
    {
        std::cerr << "=== Image Quality Metrics ===" << std::endl;
        std::cerr << "Overall Score:          " << overallScore << "/100" << std::endl;
        std::cerr << "Star Sharpness:         " << starSharpness << "/100" << std::endl;
        std::cerr << "Background Smoothness:  " << backgroundSmoothness << "/100" << std::endl;
        std::cerr << "Dynamic Range:          " << dynamicRange << "/100" << std::endl;
        std::cerr << "Contrast Score:         " << contrastScore << "/100" << std::endl;
        std::cerr << "Detail Preservation:    " << detailPreservation << "/100" << std::endl;
        std::cerr << "Clipping Penalty:       " << clippingPenalty << "/100" << std::endl;
        std::cerr << "SNR:                    " << snr << std::endl;
        std::cerr << "=============================" << std::endl;
    }
};

class ImageQualityEvaluator
{
public:
    
    ImageQualityEvaluator() = default;
    
    // Main evaluation function
    template <class P>
    ImageQualityMetrics Evaluate(const GenericImage<P>& image)
    {
        ImageQualityMetrics metrics;
        
        // Work with luminance for multi-channel images
        DImage workImage;
        if (image.NumberOfChannels() >= 3)
        {
            // Convert to grayscale using luminance
            workImage = DImage(image.Width(), image.Height(), ColorSpace::Gray);
            for (int y = 0; y < image.Height(); ++y)
            {
                for (int x = 0; x < image.Width(); ++x)
                {
                    double r = image.Pixel(x, y, 0);
                    double g = image.Pixel(x, y, 1);
                    double b = image.Pixel(x, y, 2);
                    // Standard luminance weights
                    workImage.Pixel(x, y, 0) = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                }
            }
        }
        else
        {
            // Already grayscale
            workImage = DImage(image);
        }
        
        // Calculate basic statistics
        CalculateStatistics(workImage, metrics);
        
        // Evaluate star sharpness
        metrics.starSharpness = EvaluateStarSharpness(workImage);
        
        // Evaluate background smoothness
        metrics.backgroundSmoothness = EvaluateBackgroundSmoothness(workImage);
        
        // Evaluate dynamic range usage
        metrics.dynamicRange = EvaluateDynamicRange(workImage, metrics);
        
        // Evaluate contrast
        metrics.contrastScore = EvaluateContrast(workImage, metrics);
        
        // Evaluate detail preservation (using gradient analysis)
        metrics.detailPreservation = EvaluateDetailPreservation(workImage);
        
        // Calculate clipping penalty
        metrics.clippingPenalty = CalculateClippingPenalty(workImage);
        
        // Calculate overall score (weighted combination)
        metrics.overallScore = CalculateOverallScore(metrics);
        
        return metrics;
    }
    
    // Compare two images (e.g., before and after processing)
    template <class P>
    double CompareImprovement(const GenericImage<P>& before, const GenericImage<P>& after)
    {
        ImageQualityMetrics beforeMetrics = Evaluate(before);
        ImageQualityMetrics afterMetrics = Evaluate(after);
        
        return afterMetrics.overallScore - beforeMetrics.overallScore;
    }
    
private:
    
    void CalculateStatistics(const DImage& image, ImageQualityMetrics& metrics)
    {
        const int width = image.Width();
        const int height = image.Height();
        const int numPixels = width * height;
        
        // Calculate mean
        double sum = 0.0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                sum += image.Pixel(x, y, 0);
        
        metrics.meanValue = sum / numPixels;
        
        // Calculate standard deviation
        double sumSquaredDiff = 0.0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                double diff = image.Pixel(x, y, 0) - metrics.meanValue;
                sumSquaredDiff += diff * diff;
            }
        
        metrics.stdDev = std::sqrt(sumSquaredDiff / numPixels);
        
        // Calculate median (sample-based for efficiency)
        std::vector<double> samples;
        samples.reserve(std::min(10000, numPixels));
        
        int sampleStep = std::max(1, numPixels / 10000);
        for (int y = 0; y < height; y += sampleStep)
            for (int x = 0; x < width; x += sampleStep)
                samples.push_back(image.Pixel(x, y, 0));
        
        std::nth_element(samples.begin(), samples.begin() + samples.size() / 2, samples.end());
        metrics.medianValue = samples[samples.size() / 2];
        
        // Calculate SNR (signal-to-noise ratio)
        if (metrics.stdDev > 0)
            metrics.snr = 20.0 * std::log10(metrics.meanValue / metrics.stdDev);
        else
            metrics.snr = 100.0; // Perfect signal
    }
    
    double EvaluateStarSharpness(const DImage& image)
    {
        const int width = image.Width();
        const int height = image.Height();
        
        // Use Sobel operator to detect edges (stars should have sharp edges)
        double totalGradient = 0.0;
        int brightPixelCount = 0;
        const double brightnessThreshold = 0.3; // Consider pixels above 30% as potential stars
        
        for (int y = 1; y < height - 1; ++y)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                double centerPixel = image.Pixel(x, y, 0);
                
                // Only evaluate bright areas (likely stars)
                if (centerPixel > brightnessThreshold)
                {
                    // Sobel X
                    double gx = -image.Pixel(x-1, y-1, 0) + image.Pixel(x+1, y-1, 0)
                              -2*image.Pixel(x-1, y, 0) + 2*image.Pixel(x+1, y, 0)
                              -image.Pixel(x-1, y+1, 0) + image.Pixel(x+1, y+1, 0);
                    
                    // Sobel Y
                    double gy = -image.Pixel(x-1, y-1, 0) - 2*image.Pixel(x, y-1, 0) - image.Pixel(x+1, y-1, 0)
                              +image.Pixel(x-1, y+1, 0) + 2*image.Pixel(x, y+1, 0) + image.Pixel(x+1, y+1, 0);
                    
                    double gradient = std::sqrt(gx * gx + gy * gy);
                    totalGradient += gradient;
                    brightPixelCount++;
                }
            }
        }
        
        if (brightPixelCount == 0)
            return 0.0;
        
        double avgGradient = totalGradient / brightPixelCount;
        
        // Normalize to 0-100 scale (empirically determined scaling)
        return std::min(100.0, avgGradient * 200.0);
    }
    
    double EvaluateBackgroundSmoothness(const DImage& image)
    {
        const int width = image.Width();
        const int height = image.Height();
        
        // Evaluate smoothness in dark areas (background)
        double totalVariation = 0.0;
        int backgroundPixelCount = 0;
        const double backgroundThreshold = 0.2; // Consider pixels below 20% as background
        
        for (int y = 1; y < height - 1; ++y)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                double centerPixel = image.Pixel(x, y, 0);
                
                if (centerPixel < backgroundThreshold)
                {
                    // Calculate local variation
                    double variation = 0.0;
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (dx == 0 && dy == 0) continue;
                            double diff = centerPixel - image.Pixel(x + dx, y + dy, 0);
                            variation += std::abs(diff);
                        }
                    }
                    
                    totalVariation += variation;
                    backgroundPixelCount++;
                }
            }
        }
        
        if (backgroundPixelCount == 0)
            return 50.0;
        
        double avgVariation = totalVariation / backgroundPixelCount;
        
        // Lower variation = smoother background = higher score
        // Normalize to 0-100 scale (lower variation is better)
        return std::max(0.0, 100.0 - avgVariation * 500.0);
    }
    
    double EvaluateDynamicRange(const DImage& image, ImageQualityMetrics& metrics)
    {
        // Build histogram
        const int numBins = 256;
        std::vector<int> histogram(numBins, 0);
        
        const int width = image.Width();
        const int height = image.Height();
        
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                double value = image.Pixel(x, y, 0);
                int bin = std::min(numBins - 1, static_cast<int>(value * numBins));
                histogram[bin]++;
            }
        }
        
        // Find the range that contains 98% of pixels (ignore extreme outliers)
        int totalPixels = width * height;
        int threshold = totalPixels * 0.01; // 1% on each end
        
        int lowBin = 0, highBin = numBins - 1;
        int cumulative = 0;
        for (int i = 0; i < numBins; ++i)
        {
            cumulative += histogram[i];
            if (cumulative > threshold)
            {
                lowBin = i;
                break;
            }
        }
        
        cumulative = 0;
        for (int i = numBins - 1; i >= 0; --i)
        {
            cumulative += histogram[i];
            if (cumulative > threshold)
            {
                highBin = i;
                break;
            }
        }
        
        // Calculate utilization
        double utilizedRange = static_cast<double>(highBin - lowBin) / numBins;
        
        // Store histogram info
        metrics.histogramSpread = utilizedRange;
        
        // Higher utilization = better score
        return utilizedRange * 100.0;
    }
    
    double EvaluateContrast(const DImage& image, const ImageQualityMetrics& metrics)
    {
        // Use Michelson contrast: (max - min) / (max + min)
        // But weight it by how spread out the values are
        
        double min = 1.0, max = 0.0;
        const int width = image.Width();
        const int height = image.Height();
        
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                double value = image.Pixel(x, y, 0);
                min = std::min(min, value);
                max = std::max(max, value);
            }
        }
        
        double michelsonContrast = (max - min) / (max + min + 1e-10);
        
        // Combine with standard deviation (another measure of contrast)
        double normalizedStdDev = metrics.stdDev / 0.3; // 0.3 is a good typical value
        
        // Weighted combination
        double contrastScore = (michelsonContrast * 50.0 + normalizedStdDev * 50.0);
        
        return std::min(100.0, contrastScore);
    }
    
    double EvaluateDetailPreservation(const DImage& image)
    {
        // Use high-pass filtering to detect fine details
        const int width = image.Width();
        const int height = image.Height();
        
        double totalHighFreq = 0.0;
        int pixelCount = 0;
        
        // Simple Laplacian operator for edge detection
        for (int y = 1; y < height - 1; ++y)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                double center = image.Pixel(x, y, 0);
                double laplacian = -4 * center
                                 + image.Pixel(x-1, y, 0)
                                 + image.Pixel(x+1, y, 0)
                                 + image.Pixel(x, y-1, 0)
                                 + image.Pixel(x, y+1, 0);
                
                totalHighFreq += std::abs(laplacian);
                pixelCount++;
            }
        }
        
        double avgHighFreq = totalHighFreq / pixelCount;
        
        // Normalize to 0-100 scale
        return std::min(100.0, avgHighFreq * 300.0);
    }
    
    double CalculateClippingPenalty(const DImage& image)
    {
        const int width = image.Width();
        const int height = image.Height();
        const int totalPixels = width * height;
        
        int clippedBlack = 0;
        int clippedWhite = 0;
        
        const double blackThreshold = 0.001;
        const double whiteThreshold = 0.999;
        
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                double value = image.Pixel(x, y, 0);
                if (value < blackThreshold) clippedBlack++;
                if (value > whiteThreshold) clippedWhite++;
            }
        }
        
        double clippedPercent = (static_cast<double>(clippedBlack + clippedWhite) / totalPixels) * 100.0;
        
        // Heavy penalty for clipping (anything over 1% is bad)
        double penalty = std::max(0.0, 100.0 - clippedPercent * 50.0);
        
        return penalty;
    }
    
    double CalculateOverallScore(const ImageQualityMetrics& metrics)
    {
        // Weighted combination of all metrics
        // Adjust these weights based on what's most important for your use case
        
        double score = 0.0;
        score += metrics.starSharpness * 0.25;         // 25% weight
        score += metrics.backgroundSmoothness * 0.15;  // 15% weight
        score += metrics.dynamicRange * 0.20;          // 20% weight
        score += metrics.contrastScore * 0.15;         // 15% weight
        score += metrics.detailPreservation * 0.15;    // 15% weight
        score += metrics.clippingPenalty * 0.10;       // 10% weight
        
        return score;
    }
};

} // namespace pcl

#endif // __ImageQualityEvaluator_h
