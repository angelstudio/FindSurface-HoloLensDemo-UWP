#include "pch.h"
#include "HolographicFindSurfaceDemoMain.h"
#include "Common/DirectXHelper.h"

#include <windows.graphics.directx.direct3d11.interop.h>

#ifdef DRAW_SAMPLE_CONTENT
#include <ppltasks.h> // async_task
#include <sstream> // for Debug String

#include "Helper.h" // Picking
#endif

using namespace HolographicFindSurfaceDemo;
using namespace concurrency;
using namespace Microsoft::WRL;
using namespace std::placeholders;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Gaming::Input;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::UI::Input::Spatial;
using namespace winrt::Windows::Foundation::Metadata;

#ifdef DRAW_SAMPLE_CONTENT
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Perception;
// Eye Gaze
using namespace winrt::Windows::Perception::People;
using namespace winrt::Windows::UI::Input;
// Speech Recognition
using namespace winrt::Windows::Media::SpeechRecognition;
// Speech Synthesis (voice output)
using namespace winrt::Windows::Media::SpeechSynthesis;
// Define Command ID
#define VCID_NONE              0x00
#define VCID_FIND_ANY          0x01
#define VCID_FIND_PLANE        0x02
#define VCID_FIND_SPHERE       0x03
#define VCID_FIND_CYLINDER     0x04
#define VCID_FIND_CONE         0x05
#define VCID_FIND_TORUS        0x06
#define VCID_STOP              0x10
#define VCID_CAPTURE           0x20
#define VCID_UNDO              0x21
#define VCID_CLEAR             0x22
#define VCID_SHOW_POINTCLOUD   0x30
#define VCID_HIDE_POINTCLOUD   0x31
#define VCID_SHOW_CURSOR       0x32
#define VCID_HIDE_CURSOR       0x33
#define VCID_SHOW_ALL          0x34
#define VCID_HIDE_ALL          0x35
#define VCID_VERY_SMALL_RADIUS 0x40
#define VCID_SMALL_RADIUS      0x41
#define VCID_NORMAL_RADIUS     0x42
#define VCID_LARGE_RADIUS      0x43
#define VCID_VERY_LARGE_RADIUS 0x44
#define VCID_NORMAL_ERROR      0x50
#define VCID_HIGH_ERROR        0x51
#define VCID_LOW_ERROR         0x52
#endif

// Loads and initializes application assets when the application is loaded.
HolographicFindSurfaceDemoMain::HolographicFindSurfaceDemoMain(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    m_deviceResources(deviceResources)
{
    // Register to be notified if the device is lost or recreated.
    m_deviceResources->RegisterDeviceNotify(this);

    m_canGetHolographicDisplayForCamera = ApiInformation::IsPropertyPresent(winrt::name_of<HolographicCamera>(), L"Display");
    m_canGetDefaultHolographicDisplay = ApiInformation::IsMethodPresent(winrt::name_of<HolographicDisplay>(), L"GetDefault");
    m_canCommitDirect3D11DepthBuffer = ApiInformation::IsMethodPresent(winrt::name_of<HolographicCameraRenderingParameters>(), L"CommitDirect3D11DepthBuffer");
    m_canUseWaitForNextFrameReadyAPI = ApiInformation::IsMethodPresent(winrt::name_of<HolographicSpace>(), L"WaitForNextFrameReady");

    if (m_canGetDefaultHolographicDisplay)
    {
        // Subscribe for notifications about changes to the state of the default HolographicDisplay 
        // and its SpatialLocator.
        m_holographicDisplayIsAvailableChangedEventToken = HolographicSpace::IsAvailableChanged(bind(&HolographicFindSurfaceDemoMain::OnHolographicDisplayIsAvailableChanged, this, _1, _2));
    }

    // Acquire the current state of the default HolographicDisplay and its SpatialLocator.
    OnHolographicDisplayIsAvailableChanged(nullptr, nullptr);

#ifdef DRAW_SAMPLE_CONTENT
    // Initialize the map of speech commands to actions.
    InitializeSpeechCommandList();

    // Initialize Voice UI(s).
    InitializeVoiceUIPrompt();

    // Initialize FindSurface Context (singleton instance)
    m_pFS = FindSurface::getInstance();
#endif
}

#ifdef DRAW_SAMPLE_CONTENT

//> BEGIN - SpeechRecognition Related
// Initializes the speech command list.
void HolographicFindSurfaceDemoMain::InitializeSpeechCommandList()
{
    m_lastCommandId = VCID_NONE;
    m_speechCommandData = winrt::single_threaded_map<winrt::hstring, int>();

    // Find Command
    m_speechCommandData.Insert(L"find any", VCID_FIND_ANY);
    m_speechCommandData.Insert(L"find plane", VCID_FIND_PLANE);
    m_speechCommandData.Insert(L"find sphere", VCID_FIND_SPHERE);
    m_speechCommandData.Insert(L"find ball", VCID_FIND_SPHERE);
    m_speechCommandData.Insert(L"find cylinder", VCID_FIND_CYLINDER);
    m_speechCommandData.Insert(L"find cone", VCID_FIND_CONE);
    m_speechCommandData.Insert(L"find torus", VCID_FIND_TORUS);
    m_speechCommandData.Insert(L"find donut", VCID_FIND_TORUS);
    // Stop
    m_speechCommandData.Insert(L"stop", VCID_STOP);
    // Manipulate Model Cache
    m_speechCommandData.Insert(L"capture", VCID_CAPTURE);
    m_speechCommandData.Insert(L"catch it", VCID_CAPTURE);
    m_speechCommandData.Insert(L"catch that", VCID_CAPTURE);
    m_speechCommandData.Insert(L"undo", VCID_UNDO);
    m_speechCommandData.Insert(L"remove last one", VCID_UNDO);
    m_speechCommandData.Insert(L"clear", VCID_CLEAR);
    m_speechCommandData.Insert(L"remove all", VCID_CLEAR);
    // Show / Hide Point Cloud
    m_speechCommandData.Insert(L"show point cloud", VCID_SHOW_POINTCLOUD);
    m_speechCommandData.Insert(L"hide point cloud", VCID_HIDE_POINTCLOUD);
    // Show / Hide UI
    m_speechCommandData.Insert(L"show cursor", VCID_SHOW_CURSOR);
    m_speechCommandData.Insert(L"show gaze cursor", VCID_SHOW_CURSOR);
    m_speechCommandData.Insert(L"show gaze point", VCID_SHOW_CURSOR);
    m_speechCommandData.Insert(L"show UI", VCID_SHOW_CURSOR);
    m_speechCommandData.Insert(L"hide cursor", VCID_HIDE_CURSOR);
    m_speechCommandData.Insert(L"hide gaze cursor", VCID_HIDE_CURSOR);
    m_speechCommandData.Insert(L"hide gaze point", VCID_HIDE_CURSOR);
    m_speechCommandData.Insert(L"hide UI", VCID_HIDE_CURSOR);
    // Show / Hide All?
    m_speechCommandData.Insert(L"show all", VCID_SHOW_ALL);
    m_speechCommandData.Insert(L"hide all", VCID_HIDE_ALL);
    // Quick Setting of Seed Radius
    m_speechCommandData.Insert(L"very small size", VCID_VERY_SMALL_RADIUS);
    m_speechCommandData.Insert(L"very small one", VCID_VERY_SMALL_RADIUS);
    m_speechCommandData.Insert(L"small size", VCID_SMALL_RADIUS);
    m_speechCommandData.Insert(L"small one", VCID_SMALL_RADIUS);
    m_speechCommandData.Insert(L"normal size", VCID_NORMAL_RADIUS);
    m_speechCommandData.Insert(L"normal one", VCID_NORMAL_RADIUS);
    m_speechCommandData.Insert(L"general size", VCID_NORMAL_RADIUS);
    m_speechCommandData.Insert(L"medium size", VCID_NORMAL_RADIUS);
    m_speechCommandData.Insert(L"reset size", VCID_NORMAL_RADIUS);
    m_speechCommandData.Insert(L"large size", VCID_LARGE_RADIUS);
    m_speechCommandData.Insert(L"large one", VCID_LARGE_RADIUS);
    m_speechCommandData.Insert(L"very large size", VCID_VERY_LARGE_RADIUS);
    m_speechCommandData.Insert(L"very large one", VCID_VERY_LARGE_RADIUS);
    // Error Adjust
    m_speechCommandData.Insert(L"normal error", VCID_NORMAL_ERROR);
    m_speechCommandData.Insert(L"high error", VCID_HIGH_ERROR);
    m_speechCommandData.Insert(L"low error", VCID_LOW_ERROR);
}

void HolographicFindSurfaceDemoMain::InitializeVoiceUIPrompt()
{
    // RecognizeWithUIAsync provides speech recognition begin and end prompts, but it does not provide
    // synthesized speech prompts. Instead, you should provide your own speech prompts when requesting
    // phrase input.
    // Here is an example of how to do that with a speech synthesizer. You could also use a pre-recorded 
    // voice clip, a visual UI, or other indicator of what to say.
    auto speechSynthesizer = winrt::Windows::Media::SpeechSynthesis::SpeechSynthesizer();

    // Kick off speech synthesis.
    create_task(
        [this, speechSynthesizer]()
        {
            // Set Voice
            auto voiceInfo = winrt::Windows::Media::SpeechSynthesis::VoiceInformation{ nullptr };
            for (auto const& voice : speechSynthesizer.AllVoices())
            {
                if (voice.Language() == L"en-US") // Find English Voice
                {
                    // Find female voice as possible as.
                    if (voice.Gender() == winrt::Windows::Media::SpeechSynthesis::VoiceGender::Female) {
                        voiceInfo = voice;
                        break;
                    }
                    else if (voiceInfo == nullptr) { // otherwise, get first voice of english
                        voiceInfo = voice;
                        // continue;
                    }
                }
            }
            if (voiceInfo != nullptr) {
                speechSynthesizer.Voice(voiceInfo);
            }

            // Voice Command List
            typedef std::pair<OmnidirectionalSound*, winrt::hstring> _VP;
            std::array voiceList = {
                _VP(&m_speechSynthesisSound, L"Voice command is ready."),
            };

            for (auto& vp : voiceList)
            {
                try
                {
                    winrt::Windows::Media::SpeechSynthesis::SpeechSynthesisStream stream{
                        speechSynthesizer.SynthesizeTextToStreamAsync(vp.second).get()
                    };

                    auto sound = vp.first;
                    auto hr = sound->Initialize(stream, 0);
                    if (SUCCEEDED(hr)) {
                        sound->SetEnvironment(HrtfEnvironment::Small);
                    }
                    else {
                        winrt::throw_hresult(hr);
                    }
                }
                catch (winrt::hresult_error const& ex)
                {
                    std::wostringstream wss;
                    wss << L"Exception while trying to synthesize speech: '"
                        << vp.second.c_str() 
                        << "': "
                        << ex.message().c_str()
                        << std::endl;
                    OutputDebugString(wss.str().c_str());
                }
            }

            // Voice UI is Ready...
            m_isVoiceUIReady = true;
        },
        task_continuation_context::get_current_winrt_context()
    );
}

void HolographicFindSurfaceDemoMain::PlayRecognitionSound()
{
    // The user should be given a cue when recognition is complete. 
    auto hr = m_recognitionSound.GetInitializationStatus();
    if (SUCCEEDED(hr)) {
        m_recognitionSound.PlaySound(); 
    }
}

// Resets the speech recognizer, if one exists.
task<void> HolographicFindSurfaceDemoMain::StopCurrentRecognizerIfExists()
{
    if (m_speechRecognizer != nullptr)
    {
        return create_task(
            [this]()
            {
                auto session = m_speechRecognizer.ContinuousRecognitionSession();
                if (session != nullptr)
                {
                    // Cancelling recognition prevents any currently recognized speech from
                    // generating a ResultGenerated event. StopAsync() will allow the final session to 
                    // complete.
                    // session.StopAsync().get();
                    session.CancelAsync().get(); // Stop and discard all pending result.
                    session.ResultGenerated(m_speechRecognizerResultEventToken);
                    m_speechRecognizerResultEventToken = {};
                }
                m_speechRecognizer.RecognitionQualityDegrading(m_speechRecognitionQualityDegradedToken);
                m_speechRecognitionQualityDegradedToken = {};

                m_speechRecognizer.Close(); // Explicit Release
                m_speechRecognizer = nullptr;
            }
            //, task_continuation_context::get_current_winrt_context()
        );
    }
    else
    {
        return create_task([this]() { });
    }
}

// Initializes a speech recognizer.
bool HolographicFindSurfaceDemoMain::InitializeSpeechRecognizer()
{
    m_speechRecognizer = SpeechRecognizer{ winrt::Windows::Globalization::Language{L"en-US"} };

    if (m_speechRecognizer == nullptr) {
        return false;
    }

    m_speechRecognitionQualityDegradedToken = m_speechRecognizer.RecognitionQualityDegrading(
        std::bind(&HolographicFindSurfaceDemoMain::OnSpeechQualityDegraded, this, _1, _2)
    );

    m_speechRecognizerResultEventToken = m_speechRecognizer.ContinuousRecognitionSession().ResultGenerated(
        std::bind(&HolographicFindSurfaceDemoMain::OnResultGenerated, this, _1, _2)
    );

    return true;
}

// Creates a speech command recognizer, and starts listening.
task<bool> HolographicFindSurfaceDemoMain::StartRecognizeSpeechCommands()
{
    return StopCurrentRecognizerIfExists().then(
        [this]()
        {
            if (!InitializeSpeechRecognizer()) {
                return task_from_result<bool>(false);
            }

            // Here, we compile the list of voice commands.
            winrt::Windows::Foundation::Collections::IVector<winrt::hstring> commandList{
                winrt::single_threaded_vector<winrt::hstring>()
            };

            for (auto pair : m_speechCommandData)
            {
                commandList.Append(pair.Key());
            }

            SpeechRecognitionListConstraint spConstraint{ commandList };
            m_speechRecognizer.Constraints().Clear();
            m_speechRecognizer.Constraints().Append(spConstraint);

            return create_task(
                [this]()
                {
                    try 
                    {
                        SpeechRecognitionCompilationResult compilationResult{
                            m_speechRecognizer.CompileConstraintsAsync().get()
                        };

                        if (compilationResult.Status() == SpeechRecognitionResultStatus::Success)
                        {
                            // If compilation succeeds, we can start listening for results.
                            return create_task(
                                [this]()
                                {
                                    try
                                    {
                                        m_speechRecognizer.ContinuousRecognitionSession().StartAsync().get();
                                        return true;
                                    }
                                    catch (winrt::hresult_error const& ex)
                                    {
#ifdef _DEBUG
                                        {
                                            std::wostringstream wss;
                                            wss << L"Exception while trying to start speech Recognition: " << ex.message().c_str() << std::endl;
                                            OutputDebugString(wss.str().c_str());
                                        }
#endif
                                        return false;
                                    }
                                }
                            );
                        }
                        else
                        {
                            OutputDebugString(L"Could not initialize constraint-based speech engine!\n");

                            // Handle error here.
                            return create_task([this] { return false; });
                        }
                        
                    }
                    catch (winrt::hresult_error const& ex)
                    {
                        // Note that if you get an "Access is denied" exception, you might need to enable the microphone 
                        // privacy setting on the device and/or add the microphone capability to your app manifest.
#ifdef _DEBUG
                        {
                            std::wostringstream wss;
                            wss << L"Exception while trying to initialize speech command list: " << ex.message().c_str() << std::endl;
                            OutputDebugString(wss.str().c_str());
                        }
#endif
                        // Handle exceptions here.
                        return create_task([this] { return false; });
                    }
                }
            );
        }
    );
}
//> END - SpeechRecognition Related

void HolographicFindSurfaceDemoMain::HandlePointCloudStream()
{
    std::vector<DirectX::XMFLOAT3> buffer;
    long long timestamp = 0;

    if (m_pSM->getUpdatedData(buffer, timestamp))
    {
        auto locator = m_pSM->spatialLocator();
        auto pts = PerceptionTimestampHelper::FromSystemRelativeTargetTime(HundredsOfNanoseconds(timestamp));

        auto location = locator.TryLocateAtTimestamp(pts, m_stationaryReferenceFrame.CoordinateSystem());
        float4x4 rigNodeToCoordinateSystem = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());

        DirectX::XMMATRIX c2r = DirectX::XMLoadFloat4x4(m_pSM->getCameraNodeToRigNode());
        DirectX::XMMATRIX r2g = DirectX::XMLoadFloat4x4(&rigNodeToCoordinateSystem);
        DirectX::XMMATRIX pointCloudModel = DirectX::XMMatrixMultiply(c2r, r2g);

        // Update to member variable
        m_vecPrevPCData.swap(buffer);
        DirectX::XMStoreFloat4x4(&m_matPrevPCModel, pointCloudModel);
        m_nPrevPCTimestamp = timestamp;

        m_pointCloudRenderer->UpdatePointCloudBuffer(m_vecPrevPCData.data(), m_vecPrevPCData.size(), pointCloudModel);
    }
}

void HolographicFindSurfaceDemoMain::HandleVoiceCommand()
{
    if (m_lastCommandId)
    {
        int cmdId = m_lastCommandId;
        m_lastCommandId = 0; // Consume

        // Handle Voice Command
        switch (cmdId)
        {
        case VCID_FIND_ANY:
            m_runFindSurface = true;
            m_findType = FS_TYPE_ANY;
            break;
        case VCID_FIND_PLANE:
            m_runFindSurface = true;
            m_findType = FS_TYPE_PLANE;
            break;
        case VCID_FIND_SPHERE:
            m_runFindSurface = true;
            m_findType = FS_TYPE_SPHERE;
            break;
        case VCID_FIND_CYLINDER:
            m_runFindSurface = true;
            m_findType = FS_TYPE_CYLINDER;
            break;
        case VCID_FIND_CONE:
            m_runFindSurface = true;
            m_findType = FS_TYPE_CONE;
            break;
        case VCID_FIND_TORUS:
            m_runFindSurface = true;
            m_findType = FS_TYPE_TORUS;
            break;
        case VCID_STOP:
            m_runFindSurface = false;
            m_meshRenderer->ClearCurrentModel();
            break;
        case VCID_CAPTURE:
            m_meshRenderer->StoreCurrent();
            break;
        case VCID_UNDO:
            m_meshRenderer->RemoveLast();
            break;
        case VCID_CLEAR:
            m_meshRenderer->RemoveAll();
            break;
        case VCID_SHOW_POINTCLOUD:
            m_isShowPointCloud = true;
            break;
        case VCID_HIDE_POINTCLOUD:
            m_isShowPointCloud = false;
            break;
        case VCID_SHOW_CURSOR:
            m_isShowCursor = true;
            break;
        case VCID_HIDE_CURSOR:
            m_isShowCursor = false;
            break;
        case VCID_SHOW_ALL:
            m_isShowPointCloud = true;
            m_isShowCursor = true;
            break;
        case VCID_HIDE_ALL:
            m_isShowPointCloud = false;
            m_isShowCursor = false;
            break;
        case VCID_VERY_SMALL_RADIUS:
            m_gazePointRenderer->SetSeedRadiusAtMeter(0.025f);
            break;
        case VCID_SMALL_RADIUS:
            m_gazePointRenderer->SetSeedRadiusAtMeter(0.05f);
            break;
        case VCID_NORMAL_RADIUS:
            m_gazePointRenderer->SetSeedRadiusAtMeter(0.1f);
            break;
        case VCID_LARGE_RADIUS:
            m_gazePointRenderer->SetSeedRadiusAtMeter(0.2f);
            break;
        case VCID_VERY_LARGE_RADIUS:
            m_gazePointRenderer->SetSeedRadiusAtMeter(0.4f);
            break;
        case VCID_NORMAL_ERROR:
            m_errorLevel = FindSurfaceHelper::ERROR_LEVEL_NORMAL;
            m_gazePointRenderer->SetCircleIndex(CIRCLE_INDEX_NORMAL);
            break;
        case VCID_HIGH_ERROR:
            m_errorLevel = FindSurfaceHelper::ERROR_LEVEL_HIGH;
            m_gazePointRenderer->SetCircleIndex(CIRCLE_INDEX_HIGH);
            break;
        case VCID_LOW_ERROR:
            m_errorLevel = FindSurfaceHelper::ERROR_LEVEL_LOW;
            m_gazePointRenderer->SetCircleIndex(CIRCLE_INDEX_LOW);
            break;
        }

        m_gazePointRenderer->SetRotateSpeed(m_runFindSurface ? ROTATE_FAST_SPEED : ROTATE_NORMAL_SPEED);
    }
}

bool HolographicFindSurfaceDemoMain::GetGazeInput(const SpatialPointerPose& pose, float3& outOrigin, float3& outDirection)
{
    // Use Eye-gaze, if possible
    if (m_isEyeTrackingEnabled)
    {
        auto eyes = pose.Eyes();
        if (eyes && eyes.IsCalibrationValid())
        {
            auto gaze = eyes.Gaze();
            if (gaze)
            {
                auto spatialRay = gaze.Value();
                outOrigin = spatialRay.Origin;
                outDirection = spatialRay.Direction;

                return true;
            }
        }
    }
    // otherwise, try using Hand Pointing, if possible
    else if (m_spatialInteractionManager != nullptr)
    {
        SpatialInteractionSourceState targetSourceState = nullptr;

        auto sourceStates = m_spatialInteractionManager.GetDetectedSourcesAtTimestamp(pose.Timestamp());
        for (auto sourceState : sourceStates)
        {
            // Get Any Hand (we don't know which is left or right hand)
            auto source = sourceState.Source();
            if (source.Kind() == SpatialInteractionSourceKind::Hand && source.IsPointingSupported())
            {
                // find hand that is used last frame as possible
                if (source.Id() == m_lastHandId)
                {
                    targetSourceState = sourceState;
                    break;
                }
                else if (targetSourceState == nullptr)
                {
                    targetSourceState = sourceState;
                }
            }
        }

        if (targetSourceState != nullptr)
        {
            auto handPose = targetSourceState.Properties().TryGetLocation(m_stationaryReferenceFrame.CoordinateSystem()).SourcePointerPose();
            m_lastHandId = targetSourceState.Source().Id();

            outOrigin = handPose.Position();
            outDirection = handPose.ForwardDirection();

            return true;
        }
        else
        {
            m_lastHandId = 0;
        }
    }

    // By default, use head gaze
    outOrigin = pose.Head().Position();
    outDirection = pose.Head().ForwardDirection();

    return false;
}
#endif

void HolographicFindSurfaceDemoMain::SetHolographicSpace(HolographicSpace const& holographicSpace)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = holographicSpace;

    m_spatialInteractionManager = SpatialInteractionManager::GetForCurrentView();

    //
    // TODO: Add code here to initialize your holographic content.
    //

#ifdef DRAW_SAMPLE_CONTENT
    // Request Eye-gaze input Permissions
    auto eyePerm = PermissionHelper::RequestEyeTrackingPermissionAsync();
    eyePerm.then([this](bool result) { m_isEyeTrackingEnabled = result; });

    // Initialize the sample hologram.
    m_gazePointRenderer = std::make_unique<GazePointRenderer>(m_deviceResources);

    m_pointCloudRenderer = std::make_unique<PointCloudRenderer>(m_deviceResources);
    m_meshRenderer = std::make_unique<MeshRenderer>(m_deviceResources);
    // Initialize the Sensor Manager.
    m_pSM = std::make_unique<SensorManager>();
    m_pSM->initializeSensor();
    m_pSM->startSensor();

    // Preload audio assets for audio cues.
    HRESULT hr;
    hr = m_recognitionSound.Initialize(L"Audio//BasicResultsEarcon.wav", 0);
    if (SUCCEEDED(hr)) { m_recognitionSound.SetEnvironment(HrtfEnvironment::Small); }

    auto micPerm = PermissionHelper::RequestMicrophonePermissionAsyc();
    micPerm.then(
        [this](bool result) 
        {
            m_isMicAvailable = result;
            if (m_isMicAvailable)
            {
                // Start Speech Recognizer
                StartRecognizeSpeechCommands().then(
                    [this](bool result)
                    {
                        if (result && m_isVoiceUIReady) // if speech recognizer started successfully
                        {
                            auto hr = m_speechSynthesisSound.GetInitializationStatus();
                            if (SUCCEEDED(hr)) {
                                m_speechSynthesisSound.PlaySound();
                            }
                        }
                    }
                );
            }
        }
    );
    
#endif

    // Respond to camera added events by creating any resources that are specific
    // to that camera, such as the back buffer render target view.
    // When we add an event handler for CameraAdded, the API layer will avoid putting
    // the new camera in new HolographicFrames until we complete the deferral we created
    // for that handler, or return from the handler without creating a deferral. This
    // allows the app to take more than one frame to finish creating resources and
    // loading assets for the new holographic camera.
    // This function should be registered before the app creates any HolographicFrames.
    m_cameraAddedToken = m_holographicSpace.CameraAdded(std::bind(&HolographicFindSurfaceDemoMain::OnCameraAdded, this, _1, _2));

    // Respond to camera removed events by releasing resources that were created for that
    // camera.
    // When the app receives a CameraRemoved event, it releases all references to the back
    // buffer right away. This includes render target views, Direct2D target bitmaps, and so on.
    // The app must also ensure that the back buffer is not attached as a render target, as
    // shown in DeviceResources::ReleaseResourcesForBackBuffer.
    m_cameraRemovedToken = m_holographicSpace.CameraRemoved(std::bind(&HolographicFindSurfaceDemoMain::OnCameraRemoved, this, _1, _2));

    // Notes on spatial tracking APIs:
    // * Stationary reference frames are designed to provide a best-fit position relative to the
    //   overall space. Individual positions within that reference frame are allowed to drift slightly
    //   as the device learns more about the environment.
    // * When precise placement of individual holograms is required, a SpatialAnchor should be used to
    //   anchor the individual hologram to a position in the real world - for example, a point the user
    //   indicates to be of special interest. Anchor positions do not drift, but can be corrected; the
    //   anchor will use the corrected position starting in the next frame after the correction has
    //   occurred.
}

void HolographicFindSurfaceDemoMain::UnregisterHolographicEventHandlers()
{
    if (m_holographicSpace != nullptr)
    {
        // Clear previous event registrations.
        m_holographicSpace.CameraAdded(m_cameraAddedToken);
        m_cameraAddedToken = {};
        m_holographicSpace.CameraRemoved(m_cameraRemovedToken);
        m_cameraRemovedToken = {};
    }

    if (m_spatialLocator != nullptr)
    {
        m_spatialLocator.LocatabilityChanged(m_locatabilityChangedToken);
    }
}

HolographicFindSurfaceDemoMain::~HolographicFindSurfaceDemoMain()
{
#ifdef DRAW_SAMPLE_CONTENT
    if (m_pSM) { m_pSM->stopSensor(); }
#endif

    // Deregister device notification.
    m_deviceResources->RegisterDeviceNotify(nullptr);

    UnregisterHolographicEventHandlers();

    HolographicSpace::IsAvailableChanged(m_holographicDisplayIsAvailableChangedEventToken);
}

// Updates the application state once per frame.
HolographicFrame HolographicFindSurfaceDemoMain::Update(HolographicFrame const& previousFrame)
{
    // TODO: Put CPU work that does not depend on the HolographicCameraPose here.

    // Apps should wait for the optimal time to begin pose-dependent work.
    // The platform will automatically adjust the wakeup time to get
    // the lowest possible latency at high frame rates. For manual
    // control over latency, use the WaitForNextFrameReadyWithHeadStart 
    // API.
    // WaitForNextFrameReady and WaitForNextFrameReadyWithHeadStart are the
    // preferred frame synchronization APIs for Windows Mixed Reality. When 
    // running on older versions of the OS that do not include support for
    // these APIs, your app can use the WaitForFrameToFinish API for similar 
    // (but not as optimal) behavior.
    if (m_canUseWaitForNextFrameReadyAPI)
    {
        try
        {
            m_holographicSpace.WaitForNextFrameReady();
        }
        catch (winrt::hresult_not_implemented const& /*ex*/)
        {
            // Catch a specific case where WaitForNextFrameReady() is present but not implemented
            // and default back to WaitForFrameToFinish() in that case.
            m_canUseWaitForNextFrameReadyAPI = false;
        }
    }
    else if (previousFrame)
    {
        previousFrame.WaitForFrameToFinish();
    }

    // Before doing the timer update, there is some work to do per-frame
    // to maintain holographic rendering. First, we will get information
    // about the current frame.

    // The HolographicFrame has information that the app needs in order
    // to update and render the current frame. The app begins each new
    // frame by calling CreateNextFrame.
    HolographicFrame holographicFrame = m_holographicSpace.CreateNextFrame();

    // Get a prediction of where holographic cameras will be when this frame
    // is presented.
    HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

    // Back buffers can change from frame to frame. Validate each buffer, and recreate
    // resource views and depth buffers as needed.
    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

#ifdef DRAW_SAMPLE_CONTENT
    if (m_stationaryReferenceFrame != nullptr)
    {
        // Check for new point cloud since the last frame.
        HandlePointCloudStream();
        
        // Check for new speech input since the last frame.
        HandleVoiceCommand();

        SpatialPointerPose pose = SpatialPointerPose::TryGetAtTimestamp(m_stationaryReferenceFrame.CoordinateSystem(), prediction.Timestamp());
        // When, Point-cloud is not empty && Success to get `SpatialPointerPose`
        if (pose && !m_vecPrevPCData.empty())
        {
            // Ready to picking
            float3 gazeOrigin;
            float3 gazeDirection;
            bool useHeadGaze = !GetGazeInput(pose, gazeOrigin, gazeDirection);
            DirectX::XMMATRIX pcModel = DirectX::XMLoadFloat4x4(&m_matPrevPCModel); // Transform Matrix (PointCloud Coordinate System to StationaryFrame Coordinate System).

            // Try picking point cloud with gaze input
            int pickIdx = pickPoint(gazeOrigin, gazeDirection, m_vecPrevPCData.data(), m_vecPrevPCData.size(), pcModel);
            if (pickIdx >= 0)
            {
                float3 headPosition = pose.Head().Position();
                float3 headForward = pose.Head().ForwardDirection();
                float3 headUp = pose.Head().UpDirection();

                DirectX::XMVECTOR pickedPoint = DirectX::XMLoadFloat3(&m_vecPrevPCData[pickIdx]);
                pickedPoint = DirectX::XMVector3TransformCoord(pickedPoint, pcModel); // Transform Point Cloud Coordinates to StationaryFrame Coordinates

                float3 seedPosition;
                DirectX::XMStoreFloat3(&seedPosition, pickedPoint);

                // Update Gaze Renderer Here!!
                m_gazePointRenderer->PositionGazePointUI(pose, seedPosition);

                // Run FindSurface Here!!
                if (m_runFindSurface && !m_isFindSurfaceBusy)
                {
                    m_isFindSurfaceBusy = true;

                    // Shortest distance from head to seed point on head forward direction.
                    float distance = m_gazePointRenderer->GetHeadForwardDistanceToGazePoint();
                    float seedRadius = m_gazePointRenderer->GetSeedRadiusAtGazePoint();

                    // Set Algorithm Parameters
                    FindSurfaceHelper::FillFindSurfaceParameter(m_pFS, distance, m_errorLevel);
                    // Set PointCloud Data
                    m_pFS->setPointCloudDataFloat(m_vecPrevPCData.data(), static_cast<unsigned int>(m_vecPrevPCData.size()), sizeof(DirectX::XMFLOAT3));
                    // Run FindSurface Async
                    create_task(
                        [this, type = m_findType, pickIdx, seedRadius, headForward, headUp, pointCloudModel = m_matPrevPCModel]
                        {
                            auto result = m_pFS->findSurface(type, static_cast<unsigned int>(pickIdx), seedRadius);
                            if (result != nullptr)
                            {
                                // Result to Instance Buffer
                                InstanceConstantBuffer instance;
                                FindSurfaceHelper::FillConstantBufferFromResult(instance, result.get(), headForward, headUp, &pointCloudModel);
                                winrt::Windows::ApplicationModel::Core::CoreApplication::MainView().CoreWindow().Dispatcher().RunAsync(
                                    winrt::Windows::UI::Core::CoreDispatcherPriority::High,
                                    [this, &instance] {
                                        if (m_runFindSurface) {
                                            m_meshRenderer->SetCurrentModel(instance);
                                        }
                                        else {
                                            m_meshRenderer->ClearCurrentModel();
                                        }
                                    }
                                ).get();
                            }
                            else
                            {
                                winrt::Windows::ApplicationModel::Core::CoreApplication::MainView().CoreWindow().Dispatcher().RunAsync(
                                    winrt::Windows::UI::Core::CoreDispatcherPriority::High,
                                    [this] { m_meshRenderer->ClearCurrentModel(); }
                                ).get();
                            }

                            m_isFindSurfaceBusy = false;
                        }
                    );
                        
                }
            }
        }
    }
#endif

    m_timer.Tick([this]()
    {
        //
        // TODO: Update scene objects.
        //
        // Put time-based updates here. By default this code will run once per frame,
        // but if you change the StepTimer to use a fixed time step this code will
        // run as many times as needed to get to the current step.
        //

#ifdef DRAW_SAMPLE_CONTENT
        m_gazePointRenderer->Update(m_timer);
#endif
    });

    // On HoloLens 2, the platform can achieve better image stabilization results if it has
    // a stabilization plane and a depth buffer.
    // Note that the SetFocusPoint API includes an override which takes velocity as a 
    // parameter. This is recommended for stabilizing holograms in motion.
    for (HolographicCameraPose const& cameraPose : prediction.CameraPoses())
    {
#ifdef DRAW_SAMPLE_CONTENT
        // The HolographicCameraRenderingParameters class provides access to set
        // the image stabilization parameters.
        HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);

        // SetFocusPoint informs the system about a specific point in your scene to
        // prioritize for image stabilization. The focus point is set independently
        // for each holographic camera. When setting the focus point, put it on or 
        // near content that the user is looking at.
        // In this example, we put the focus point at the center of the sample hologram.
        // You can also set the relative velocity and facing of the stabilization
        // plane using overloads of this method.
        if (m_stationaryReferenceFrame != nullptr)
        {
            renderingParameters.SetFocusPoint(
                m_stationaryReferenceFrame.CoordinateSystem(),
                m_gazePointRenderer->GetGazePoint()
            );
        }
#endif
    }

    // The holographic frame will be used to get up-to-date view and projection matrices and
    // to present the swap chain.
    return holographicFrame;
}

// Renders the current frame to each holographic camera, according to the
// current application and spatial positioning state. Returns true if the
// frame was rendered to at least one camera.
bool HolographicFindSurfaceDemoMain::Render(HolographicFrame const& holographicFrame)
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }

    //
    // TODO: Add code for pre-pass rendering here.
    //
    // Take care of any tasks that are not specific to an individual holographic
    // camera. This includes anything that doesn't need the final view or projection
    // matrix, such as lighting maps.
    //

    // Lock the set of holographic camera resources, then draw to each camera
    // in this frame.
    return m_deviceResources->UseHolographicCameraResources<bool>(
        [this, holographicFrame](std::map<UINT32, std::unique_ptr<DX::CameraResources>>& cameraResourceMap)
    {
        // Up-to-date frame predictions enhance the effectiveness of image stablization and
        // allow more accurate positioning of holograms.
        holographicFrame.UpdateCurrentPrediction();
        HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

        bool atLeastOneCameraRendered = false;
        for (HolographicCameraPose const& cameraPose : prediction.CameraPoses())
        {
            // This represents the device-based resources for a HolographicCamera.
            DX::CameraResources* pCameraResources = cameraResourceMap[cameraPose.HolographicCamera().Id()].get();

            // Get the device context.
            const auto context = m_deviceResources->GetD3DDeviceContext();
            const auto depthStencilView = pCameraResources->GetDepthStencilView();

            // Set render targets to the current holographic camera.
            ID3D11RenderTargetView *const targets[1] = { pCameraResources->GetBackBufferRenderTargetView() };
            context->OMSetRenderTargets(1, targets, depthStencilView);

            // Clear the back buffer and depth stencil view.
            if (m_canGetHolographicDisplayForCamera &&
                cameraPose.HolographicCamera().Display().IsOpaque())
            {
                context->ClearRenderTargetView(targets[0], DirectX::Colors::CornflowerBlue);
            }
            else
            {
                context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
            }
            context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            //
            // TODO: Replace the sample content with your own content.
            //
            // Notes regarding holographic content:
            //    * For drawing, remember that you have the potential to fill twice as many pixels
            //      in a stereoscopic render target as compared to a non-stereoscopic render target
            //      of the same resolution. Avoid unnecessary or repeated writes to the same pixel,
            //      and only draw holograms that the user can see.
            //    * To help occlude hologram geometry, you can create a depth map using geometry
            //      data obtained via the surface mapping APIs. You can use this depth map to avoid
            //      rendering holograms that are intended to be hidden behind tables, walls,
            //      monitors, and so on.
            //    * On HolographicDisplays that are transparent, black pixels will appear transparent 
            //      to the user. On such devices, you should clear the screen to Transparent as shown 
            //      above. You should still use alpha blending to draw semitransparent holograms. 
            //


            // The view and projection matrices for each holographic camera will change
            // every frame. This function refreshes the data in the constant buffer for
            // the holographic camera indicated by cameraPose.
            if (m_stationaryReferenceFrame)
            {
                pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, m_stationaryReferenceFrame.CoordinateSystem());
            }

            // Attach the view/projection constant buffer for this camera to the graphics pipeline.
            bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

#ifdef DRAW_SAMPLE_CONTENT
            // Only render world-locked content when positional tracking is active.
            if (cameraActive)
            {
                // Draw the sample hologram.
                if (m_isShowCursor) { m_gazePointRenderer->Render(); }
                if (m_isShowPointCloud) { m_pointCloudRenderer->Render(); }
                m_meshRenderer->Render();
                if (m_canCommitDirect3D11DepthBuffer)
                {
                    // On versions of the platform that support the CommitDirect3D11DepthBuffer API, we can 
                    // provide the depth buffer to the system, and it will use depth information to stabilize 
                    // the image at a per-pixel level.
                    HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);
                    
                    IDirect3DSurface interopSurface = DX::CreateDepthTextureInteropObject(pCameraResources->GetDepthStencilTexture2D());

                    // Calling CommitDirect3D11DepthBuffer causes the system to queue Direct3D commands to 
                    // read the depth buffer. It will then use that information to stabilize the image as
                    // the HolographicFrame is presented.
                    renderingParameters.CommitDirect3D11DepthBuffer(interopSurface);
                }
            }
#endif
            atLeastOneCameraRendered = true;
        }

        return atLeastOneCameraRendered;
    });
}

void HolographicFindSurfaceDemoMain::SaveAppState()
{
    //
    // TODO: Insert code here to save your app state.
    //       This method is called when the app is about to suspend.
    //
    //       For example, store information in the SpatialAnchorStore.
    //
}

void HolographicFindSurfaceDemoMain::LoadAppState()
{
    //
    // TODO: Insert code here to load your app state.
    //       This method is called when the app resumes.
    //
    //       For example, load information from the SpatialAnchorStore.
    //
}

#ifdef DRAW_SAMPLE_CONTENT
void HolographicFindSurfaceDemoMain::EnteredBackground()
{
    // This method is called when the app entered background 

    m_isAppEnteredBackground = true;
    if (m_pSM) { m_pSM->stopSensor(); }
    m_runFindSurface = false;
    m_isFindSurfaceBusy = false;
    m_meshRenderer->ClearCurrentModel();

    //StopCurrentRecognizerIfExists();
}

void HolographicFindSurfaceDemoMain::LeavingBackground()
{
    // This method is called when the app launcing or leaving background 

    if (m_isAppEnteredBackground) // Check called via actual leaving background (not launching)
    {
        m_isAppEnteredBackground = false;
        if (m_pSM) { m_pSM->startSensor(); }

        //if (m_isMicAvailable)
        //{
        //    // Start Speech Recognizer
        //    StartRecognizeSpeechCommands().then(
        //        [this](bool result)
        //        {
        //            if (result && m_isVoiceUIReady) // if speech recognizer started successfully
        //            {
        //                auto hr = m_speechSynthesisSound.GetInitializationStatus();
        //                if (SUCCEEDED(hr)) {
        //                    m_speechSynthesisSound.PlaySound();
        //                }
        //            }
        //        }
        //    );
        //}
    }
}
#endif

// Notifies classes that use Direct3D device resources that the device resources
// need to be released before this method returns.
void HolographicFindSurfaceDemoMain::OnDeviceLost()
{
#ifdef DRAW_SAMPLE_CONTENT
    m_gazePointRenderer->ReleaseDeviceDependentResources();
    m_pointCloudRenderer->ReleaseDeviceDependentResources();
    m_meshRenderer->ReleaseDeviceDependentResources();
#endif
}

// Notifies classes that use Direct3D device resources that the device resources
// may now be recreated.
void HolographicFindSurfaceDemoMain::OnDeviceRestored()
{
#ifdef DRAW_SAMPLE_CONTENT
    m_gazePointRenderer->CreateDeviceDependentResources();
    m_pointCloudRenderer->CreateDeviceDependentResources();
    m_meshRenderer->CreateDeviceDependentResources();
#endif
}

void HolographicFindSurfaceDemoMain::OnLocatabilityChanged(SpatialLocator const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    switch (sender.Locatability())
    {
    case SpatialLocatability::Unavailable:
        // Holograms cannot be rendered.
    {
        winrt::hstring message(L"Warning! Positional tracking is " + std::to_wstring(int(sender.Locatability())) + L".\n");
        OutputDebugStringW(message.data());
    }
    break;

    // In the following three cases, it is still possible to place holograms using a
    // SpatialLocatorAttachedFrameOfReference.
    case SpatialLocatability::PositionalTrackingActivating:
        // The system is preparing to use positional tracking.

    case SpatialLocatability::OrientationOnly:
        // Positional tracking has not been activated.

    case SpatialLocatability::PositionalTrackingInhibited:
        // Positional tracking is temporarily inhibited. User action may be required
        // in order to restore positional tracking.
        break;

    case SpatialLocatability::PositionalTrackingActive:
        // Positional tracking is active. World-locked content can be rendered.
        break;
    }
}

void HolographicFindSurfaceDemoMain::OnCameraAdded(
    HolographicSpace const& sender,
    HolographicSpaceCameraAddedEventArgs const& args
)
{
    winrt::Windows::Foundation::Deferral deferral = args.GetDeferral();
    HolographicCamera holographicCamera = args.Camera();
    create_task([this, deferral, holographicCamera]()
    {
        //
        // TODO: Allocate resources for the new camera and load any content specific to
        //       that camera. Note that the render target size (in pixels) is a property
        //       of the HolographicCamera object, and can be used to create off-screen
        //       render targets that match the resolution of the HolographicCamera.
        //

        // Create device-based resources for the holographic camera and add it to the list of
        // cameras used for updates and rendering. Notes:
        //   * Since this function may be called at any time, the AddHolographicCamera function
        //     waits until it can get a lock on the set of holographic camera resources before
        //     adding the new camera. At 60 frames per second this wait should not take long.
        //   * A subsequent Update will take the back buffer from the RenderingParameters of this
        //     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
        //     Content can then be rendered for the HolographicCamera.
        m_deviceResources->AddHolographicCamera(holographicCamera);

        // Holographic frame predictions will not include any information about this camera until
        // the deferral is completed.
        deferral.Complete();
    });
}

void HolographicFindSurfaceDemoMain::OnCameraRemoved(
    HolographicSpace const& sender,
    HolographicSpaceCameraRemovedEventArgs const& args
)
{
    create_task([this]()
    {
        //
        // TODO: Asynchronously unload or deactivate content resources (not back buffer 
        //       resources) that are specific only to the camera that was removed.
        //
    });

    // Before letting this callback return, ensure that all references to the back buffer 
    // are released.
    // Since this function may be called at any time, the RemoveHolographicCamera function
    // waits until it can get a lock on the set of holographic camera resources before
    // deallocating resources for this camera. At 60 frames per second this wait should
    // not take long.
    m_deviceResources->RemoveHolographicCamera(args.Camera());
}

void HolographicFindSurfaceDemoMain::OnHolographicDisplayIsAvailableChanged(winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable)
{
    // Get the spatial locator for the default HolographicDisplay, if one is available.
    SpatialLocator spatialLocator = nullptr;
    if (m_canGetDefaultHolographicDisplay)
    {
        HolographicDisplay defaultHolographicDisplay = HolographicDisplay::GetDefault();
        if (defaultHolographicDisplay)
        {
            spatialLocator = defaultHolographicDisplay.SpatialLocator();
        }
    }
    else
    {
        spatialLocator = SpatialLocator::GetDefault();
    }

    if (m_spatialLocator != spatialLocator)
    {
        // If the spatial locator is disconnected or replaced, we should discard all state that was
        // based on it.
        if (m_spatialLocator != nullptr)
        {
            m_spatialLocator.LocatabilityChanged(m_locatabilityChangedToken);
            m_spatialLocator = nullptr;
        }

        m_stationaryReferenceFrame = nullptr;

        if (spatialLocator != nullptr)
        {
            // Use the SpatialLocator from the default HolographicDisplay to track the motion of the device.
            m_spatialLocator = spatialLocator;

            // Respond to changes in the positional tracking state.
            m_locatabilityChangedToken = m_spatialLocator.LocatabilityChanged(std::bind(&HolographicFindSurfaceDemoMain::OnLocatabilityChanged, this, _1, _2));

            // The simplest way to render world-locked holograms is to create a stationary reference frame
            // based on a SpatialLocator. This is roughly analogous to creating a "world" coordinate system
            // with the origin placed at the device's position as the app is launched.
            m_stationaryReferenceFrame = m_spatialLocator.CreateStationaryFrameOfReferenceAtCurrentLocation();
        }
    }
}

#ifdef DRAW_SAMPLE_CONTENT
// Process continuous speech recognition results.
void HolographicFindSurfaceDemoMain::OnResultGenerated(
    winrt::Windows::Media::SpeechRecognition::SpeechContinuousRecognitionSession sender,
    winrt::Windows::Media::SpeechRecognition::SpeechContinuousRecognitionResultGeneratedEventArgs args
)
{
    //winrt::Windows::ApplicationModel::Core::CoreApplication::MainView().Dispatcher().RunAsync()

    auto result = args.Result();
    if (result.Confidence() == SpeechRecognitionConfidence::High ||
        result.Confidence() == SpeechRecognitionConfidence::Medium)
    {
        auto cmdId = m_speechCommandData.TryLookup(result.Text());
        if (cmdId.has_value()) 
        {
            m_lastCommandId = cmdId.value();
            // Play a sound to indicate a command was understood.
            PlayRecognitionSound();
        }
#if _DEBUG
        {
            std::wostringstream wss;
            wss << L"Last command was: " << result.Text().c_str() << std::endl;
            OutputDebugString(wss.str().c_str());
        }
#endif
    }
    else {
        OutputDebugString(L"Recognition confidence not high enough.\n");
    }
}

// Recognize when conditions might impact speech recognition quality.
void HolographicFindSurfaceDemoMain::OnSpeechQualityDegraded(
    winrt::Windows::Media::SpeechRecognition::SpeechRecognizer recognizer,
    winrt::Windows::Media::SpeechRecognition::SpeechRecognitionQualityDegradingEventArgs args
)
{
    switch (args.Problem())
    {
    case SpeechRecognitionAudioProblem::TooFast:
        OutputDebugStringW(L"The user spoke too quickly.\n");
        break;

    case SpeechRecognitionAudioProblem::TooSlow:
        OutputDebugStringW(L"The user spoke too slowly.\n");
        break;

    case SpeechRecognitionAudioProblem::TooQuiet:
        OutputDebugStringW(L"The user spoke too softly.\n");
        break;

    case SpeechRecognitionAudioProblem::TooLoud:
        OutputDebugStringW(L"The user spoke too loudly.\n");
        break;

    case SpeechRecognitionAudioProblem::TooNoisy:
        OutputDebugStringW(L"There is too much noise in the signal.\n");
        break;

    case SpeechRecognitionAudioProblem::NoSignal:
        OutputDebugStringW(L"There is no signal.\n");
        break;

    case SpeechRecognitionAudioProblem::None:
    default:
        OutputDebugStringW(L"An error was reported with no information.\n");
        break;
    }
}
#endif
