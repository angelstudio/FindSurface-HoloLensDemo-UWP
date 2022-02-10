#pragma once

//
// Comment out this preprocessor definition to disable all of the
// sample content.
//
// To remove the content after disabling it:
//     * Remove the unused code from your app's Main class.
//     * Delete the Content folder provided with this template.
//
#define DRAW_SAMPLE_CONTENT

#include "Common/DeviceResources.h"
#include "Common/StepTimer.h"

#ifdef DRAW_SAMPLE_CONTENT
#include "Content/GazePointRenderer.h"
#include "Content/PointCloudRenderer.h"
#include "Content/MeshRenderer.h"

#include "SensorManager.h"

#include "Audio/OmnidirectionalSound.h"

#include "PermissionHelper.h"

#include <FindSurface.hpp>
#include "FindSurfaceHelper.h"
#endif

// Updates, renders, and presents holographic content using Direct3D.
namespace HolographicFindSurfaceDemo
{
    class HolographicFindSurfaceDemoMain : public DX::IDeviceNotify
    {
    public:
        HolographicFindSurfaceDemoMain(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        ~HolographicFindSurfaceDemoMain();

        // Sets the holographic space. This is our closest analogue to setting a new window
        // for the app.
        void SetHolographicSpace(winrt::Windows::Graphics::Holographic::HolographicSpace const& holographicSpace);

        // Starts the holographic frame and updates the content.
        winrt::Windows::Graphics::Holographic::HolographicFrame Update(winrt::Windows::Graphics::Holographic::HolographicFrame const& previousFrame);

        // Renders holograms, including world-locked content.
        bool Render(winrt::Windows::Graphics::Holographic::HolographicFrame const& holographicFrame);

        // Handle saving and loading of app state owned by AppMain.
        void SaveAppState();
        void LoadAppState();

#ifdef DRAW_SAMPLE_CONTENT
        // Handle background status
        void EnteredBackground();
        void LeavingBackground();
#endif

        // IDeviceNotify
        void OnDeviceLost() override;
        void OnDeviceRestored() override;

    private:
        // Asynchronously creates resources for new holographic cameras.
        void OnCameraAdded(
            winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
            winrt::Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs const& args);

        // Synchronously releases resources for holographic cameras that are no longer
        // attached to the system.
        void OnCameraRemoved(
            winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
            winrt::Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs const& args);

        // Used to notify the app when the positional tracking state changes.
        void OnLocatabilityChanged(
            winrt::Windows::Perception::Spatial::SpatialLocator const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

        // Used to respond to changes to the default spatial locator.
        void OnHolographicDisplayIsAvailableChanged(winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

        // Clears event registration state. Used when changing to a new HolographicSpace
        // and when tearing down AppMain.
        void UnregisterHolographicEventHandlers();

#ifdef DRAW_SAMPLE_CONTENT
        void HandlePointCloudStream();
        void HandleVoiceCommand();
        // Return true, if gaze source can be acquried eye or hand.
        bool GetGazeInput(
            const winrt::Windows::UI::Input::Spatial::SpatialPointerPose& pose,
            winrt::Windows::Foundation::Numerics::float3& outOrigin, 
            winrt::Windows::Foundation::Numerics::float3& outDirection
        );

        // Process continuous speech recognition results.
        void OnResultGenerated(
            winrt::Windows::Media::SpeechRecognition::SpeechContinuousRecognitionSession sender,
            winrt::Windows::Media::SpeechRecognition::SpeechContinuousRecognitionResultGeneratedEventArgs args
        );

        // Recognize when conditions might impact speech recognition quality.
        void OnSpeechQualityDegraded(
            winrt::Windows::Media::SpeechRecognition::SpeechRecognizer recognizer,
            winrt::Windows::Media::SpeechRecognition::SpeechRecognitionQualityDegradingEventArgs args
        );

        // Initializes the speech command list.
        void InitializeSpeechCommandList();

        // Initializes a speech recognizer.
        bool InitializeSpeechRecognizer();

        // Provides a voice prompt, before starting the scenario.
        void InitializeVoiceUIPrompt();
        void PlayRecognitionBeginSound();
        void PlayRecognitionSound();

        // Creates a speech command recognizer, and starts listening.
        concurrency::task<bool> StartRecognizeSpeechCommands();

        // Resets the speech recognizer, if one exists.
        concurrency::task<void> StopCurrentRecognizerIfExists();

        // Renderer
        std::unique_ptr<GazePointRenderer>                          m_gazePointRenderer;
        std::unique_ptr<PointCloudRenderer>                         m_pointCloudRenderer;
        std::unique_ptr<MeshRenderer>                               m_meshRenderer;

        // Sensor Manager
        std::unique_ptr<SensorManager>                              m_pSM;
        std::vector<DirectX::XMFLOAT3>                              m_vecPrevPCData;  // latest PointCloud Data
        DirectX::XMFLOAT4X4                                         m_matPrevPCModel; // latest PointCloud Model Matrix
        long long                                                   m_nPrevPCTimestamp = 0; // latest PointCloud Timestamp

        // Eye-gaze input
        bool                                                        m_isEyeTrackingEnabled = false;

        // Speech recognizer
        winrt::Windows::Media::SpeechRecognition::SpeechRecognizer  m_speechRecognizer = nullptr;
        winrt::Windows::Foundation::Collections::IMap<winrt::hstring, int> m_speechCommandData = nullptr;
        bool                                                        m_isMicAvailable = false;
        int                                                         m_lastCommandId = 0;

        // Handles playback of speech synthesis audio.
        OmnidirectionalSound                                        m_speechSynthesisSound;
        OmnidirectionalSound                                        m_recognitionSound;
        bool                                                        m_isVoiceUIReady = false;

        // Background Status
        bool                                                        m_isAppEnteredBackground = false;

        // For tracking hand pointing
        winrt::Windows::UI::Input::Spatial::SpatialInteractionManager m_spatialInteractionManager = nullptr;
        uint32_t                                                      m_lastHandId = 0;

        // FindSurface Related
        FindSurface                                                 *m_pFS = nullptr;
        bool                                                         m_isFindSurfaceBusy = false;
        bool                                                         m_runFindSurface = false;
        FS_FEATURE_TYPE                                              m_findType = FS_TYPE_PLANE;
        FindSurfaceHelper::ErrorLevel                                m_errorLevel = FindSurfaceHelper::ERROR_LEVEL_NORMAL;

        // Show/Hide UI Component
        bool                                                         m_isShowPointCloud = true;
        bool                                                         m_isShowCursor = true;
#endif

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>                        m_deviceResources;

        // Render loop timer.
        DX::StepTimer                                               m_timer;

        // Represents the holographic space around the user.
        winrt::Windows::Graphics::Holographic::HolographicSpace     m_holographicSpace = nullptr;

        // SpatialLocator that is attached to the default HolographicDisplay.
        winrt::Windows::Perception::Spatial::SpatialLocator         m_spatialLocator = nullptr;

        // A stationary reference frame based on m_spatialLocator.
        winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_stationaryReferenceFrame = nullptr;

        // Event registration tokens.
        winrt::event_token                                          m_cameraAddedToken;
        winrt::event_token                                          m_cameraRemovedToken;
        winrt::event_token                                          m_locatabilityChangedToken;
        winrt::event_token                                          m_holographicDisplayIsAvailableChangedEventToken;
        winrt::event_token                                          m_speechRecognizerResultEventToken;
        winrt::event_token                                          m_speechRecognitionQualityDegradedToken;

        // Cache whether or not the HolographicCamera.Display property can be accessed.
        bool                                                        m_canGetHolographicDisplayForCamera = false;

        // Cache whether or not the HolographicDisplay.GetDefault() method can be called.
        bool                                                        m_canGetDefaultHolographicDisplay = false;

        // Cache whether or not the HolographicCameraRenderingParameters.CommitDirect3D11DepthBuffer() method can be called.
        bool                                                        m_canCommitDirect3D11DepthBuffer = false;

        // Cache whether or not the HolographicFrame.WaitForNextFrameReady() method can be called.
        bool                                                        m_canUseWaitForNextFrameReadyAPI = false;
    };
}
