////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef SFML_SOUNDRECORDER_HPP
#define SFML_SOUNDRECORDER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Audio/Export.hpp>
#include <SFML/Audio/AlResource.hpp>
#include <SFML/System/Thread.hpp>
#include <SFML/System/Time.hpp>
#include <vector>
#include <string>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for capturing sound data
///
////////////////////////////////////////////////////////////
class SFML_AUDIO_API SoundRecorder : AlResource
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~SoundRecorder();

    ////////////////////////////////////////////////////////////
    /// \brief Start the capture
    ///
    /// The \a sampleRate parameter defines the number of audio samples
    /// captured per second. The higher, the better the quality
    /// (for example, 44100 samples/sec is CD quality).
    /// This function uses its own thread so that it doesn't block
    /// the rest of the program while the capture runs.
    /// Please note that only one capture can happen at the same time.
    /// You can select which capture device will be used, by passing
    /// the name to the setDevice() method. If none was selected
    /// before, the default capture device will be used. You can get a
    /// list of the names of all available capture devices by calling
    /// getAvailableDevices().
    ///
    /// \param sampleRate Desired capture rate, in number of samples per second
    ///
    /// \return True, if start of capture was successful
    ///
    /// \see stop, getAvailableDevices
    ///
    ////////////////////////////////////////////////////////////
    bool start(unsigned int sampleRate = 44100);

    ////////////////////////////////////////////////////////////
    /// \brief Stop the capture
    ///
    /// \see start
    ///
    ////////////////////////////////////////////////////////////
    void stop();

    ////////////////////////////////////////////////////////////
    /// \brief Get the sample rate
    ///
    /// The sample rate defines the number of audio samples
    /// captured per second. The higher, the better the quality
    /// (for example, 44100 samples/sec is CD quality).
    ///
    /// \return Sample rate, in samples per second
    ///
    ////////////////////////////////////////////////////////////
    unsigned int getSampleRate() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get a list of the names of all available audio capture devices
    ///
    /// This function returns a vector of strings, containing
    /// the names of all available audio capture devices.
    ///
    /// \return A vector of strings containing the names
    ///
    ////////////////////////////////////////////////////////////
    static std::vector<std::string> getAvailableDevices();

    ////////////////////////////////////////////////////////////
    /// \brief Get the name of the default audio capture device
    ///
    /// This function returns the name of the default audio
    /// capture device. If none is available, an empty string
    /// is returned.
    ///
    /// \return The name of the default audio capture device
    ///
    ////////////////////////////////////////////////////////////
    static std::string getDefaultDevice();

    ////////////////////////////////////////////////////////////
    /// \brief Set the audio capture device
    ///
    /// This function sets the audio capture device to the device
    /// with the given \a name. It can be called on the fly (i.e:
    /// while recording). If you do so while recording and
    /// opening the device fails, it stops the recording.
    ///
    /// \param name The name of the audio capture device
    ///
    /// \return True, if it was able to set the requested device
    ///
    /// \see getAvailableDevices, getDefaultDevice
    ///
    ////////////////////////////////////////////////////////////
    bool setDevice(const std::string& name);

    ////////////////////////////////////////////////////////////
    /// \brief Get the name of the current audio capture device
    ///
    /// \return The name of the current audio capture device
    ///
    ////////////////////////////////////////////////////////////
    const std::string& getDevice() const;

    ////////////////////////////////////////////////////////////
    /// \brief Set the channel count of the audio capture device
    ///
    /// This method allows you to specify the number of channels
    /// used for recording. Currently only 16-bit mono and
    /// 16-bit stereo are supported.
    ///
    /// \param channelCount Number of channels. Currently only
    ///                     mono (1) and stereo (2) are supported.
    ///
    /// \see getChannelCount
    ///
    ////////////////////////////////////////////////////////////
    void setChannelCount(unsigned int channelCount);

    ////////////////////////////////////////////////////////////
    /// \brief Get the number of channels used by this recorder
    ///
    /// Currently only mono and stereo are supported, so the
    /// value is either 1 (for mono) or 2 (for stereo).
    ///
    /// \return Number of channels
    ///
    /// \see setChannelCount
    ///
    ////////////////////////////////////////////////////////////
    unsigned int getChannelCount() const;

    ////////////////////////////////////////////////////////////
    /// \brief Check if the system supports audio capture
    ///
    /// This function should always be called before using
    /// the audio capture features. If it returns false, then
    /// any attempt to use sf::SoundRecorder or one of its derived
    /// classes will fail.
    ///
    /// \return True if audio capture is supported, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isAvailable();

protected:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// This constructor is only meant to be called by derived classes.
    ///
    ////////////////////////////////////////////////////////////
    SoundRecorder();

    ////////////////////////////////////////////////////////////
    /// \brief Set the processing interval
    ///
    /// The processing interval controls the period
    /// between calls to the onProcessSamples function. You may
    /// want to use a small interval if you want to process the
    /// recorded data in real time, for example.
    ///
    /// Note: this is only a hint, the actual period may vary.
    /// So don't rely on this parameter to implement precise timing.
    ///
    /// The default processing interval is 100 ms.
    ///
    /// \param interval Processing interval
    ///
    ////////////////////////////////////////////////////////////
    void setProcessingInterval(Time interval);

    ////////////////////////////////////////////////////////////
    /// \brief Start capturing audio data
    ///
    /// This virtual function may be overridden by a derived class
    /// if something has to be done every time a new capture
    /// starts. If not, this function can be ignored; the default
    /// implementation does nothing.
    ///
    /// \return True to start the capture, or false to abort it
    ///
    ////////////////////////////////////////////////////////////
    virtual bool onStart();

    ////////////////////////////////////////////////////////////
    /// \brief Process a new chunk of recorded samples
    ///
    /// This virtual function is called every time a new chunk of
    /// recorded data is available. The derived class can then do
    /// whatever it wants with it (storing it, playing it, sending
    /// it over the network, etc.).
    ///
    /// \param samples     Pointer to the new chunk of recorded samples
    /// \param sampleCount Number of samples pointed by \a samples
    ///
    /// \return True to continue the capture, or false to stop it
    ///
    ////////////////////////////////////////////////////////////
    virtual bool onProcessSamples(const Int16* samples, std::size_t sampleCount) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Stop capturing audio data
    ///
    /// This virtual function may be overridden by a derived class
    /// if something has to be done every time the capture
    /// ends. If not, this function can be ignored; the default
    /// implementation does nothing.
    ///
    ////////////////////////////////////////////////////////////
    virtual void onStop();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Function called as the entry point of the thread
    ///
    /// This function starts the recording loop, and returns
    /// only when the capture is stopped.
    ///
    ////////////////////////////////////////////////////////////
    void record();

    ////////////////////////////////////////////////////////////
    /// \brief Get the new available audio samples and process them
    ///
    /// This function is called continuously during the
    /// capture loop. It retrieves the captured samples and
    /// forwards them to the derived class.
    ///
    ////////////////////////////////////////////////////////////
    void processCapturedSamples();

    ////////////////////////////////////////////////////////////
    /// \brief Clean up the recorder's internal resources
    ///
    /// This function is called when the capture stops.
    ///
    ////////////////////////////////////////////////////////////
    void cleanup();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Thread             m_thread;             ///< Thread running the background recording task
    std::vector<Int16> m_samples;            ///< Buffer to store captured samples
    unsigned int       m_sampleRate;         ///< Sample rate
    Time               m_processingInterval; ///< Time period between calls to onProcessSamples
    bool               m_isCapturing;        ///< Capturing state
    std::string        m_deviceName;         ///< Name of the audio capture device
    unsigned int       m_channelCount;       ///< Number of recording channels
};

} // namespace sf


#endif // SFML_SOUNDRECORDER_HPP


////////////////////////////////////////////////////////////
/// \class sf::SoundRecorder
/// \ingroup audio
///
/// sf::SoundBuffer provides a simple interface to access
/// the audio recording capabilities of the computer
/// (the microphone). As an abstract base class, it only cares
/// about capturing sound samples, the task of making something
/// useful with them is left to the derived class. Note that
/// SFML provides a built-in specialization for saving the
/// captured data to a sound buffer (see sf::SoundBufferRecorder).
///
/// A derived class has only one virtual function to override:
/// \li onProcessSamples provides the new chunks of audio samples while the capture happens
///
/// Moreover, two additional virtual functions can be overridden
/// as well if necessary:
/// \li onStart is called before the capture happens, to perform custom initializations
/// \li onStop is called after the capture ends, to perform custom cleanup
///
/// A derived class can also control the frequency of the onProcessSamples
/// calls, with the setProcessingInterval protected function. The default
/// interval is chosen so that recording thread doesn't consume too much
/// CPU, but it can be changed to a smaller value if you need to process
/// the recorded data in real time, for example.
///
/// The audio capture feature may not be supported or activated
/// on every platform, thus it is recommended to check its
/// availability with the isAvailable() function. If it returns
/// false, then any attempt to use an audio recorder will fail.
///
/// If you have multiple sound input devices connected to your
/// computer (for example: microphone, external soundcard, webcam mic, ...)
/// you can get a list of all available devices through the
/// getAvailableDevices() function. You can then select a device
/// by calling setDevice() with the appropriate device. Otherwise
/// the default capturing device will be used.
///
/// By default the recording is in 16-bit mono. Using the
/// setChannelCount method you can change the number of channels
/// used by the audio capture device to record. Note that you
/// have to decide whether you want to record in mono or stereo
/// before starting the recording.
///
/// It is important to note that the audio capture happens in a
/// separate thread, so that it doesn't block the rest of the
/// program. In particular, the onProcessSamples virtual function
/// (but not onStart and not onStop) will be called
/// from this separate thread. It is important to keep this in
/// mind, because you may have to take care of synchronization
/// issues if you share data between threads.
/// Another thing to bear in mind is that you must call stop()
/// in the destructor of your derived class, so that the recording
/// thread finishes before your object is destroyed.
///
/// Usage example:
/// \code
/// class CustomRecorder : public sf::SoundRecorder
/// {
///     ~CustomRecorder()
///     {
///         // Make sure to stop the recording thread
///         stop();
///     }
///
///     virtual bool onStart() // optional
///     {
///         // Initialize whatever has to be done before the capture starts
///         ...
///
///         // Return true to start playing
///         return true;
///     }
///
///     virtual bool onProcessSamples(const Int16* samples, std::size_t sampleCount)
///     {
///         // Do something with the new chunk of samples (store them, send them, ...)
///         ...
///
///         // Return true to continue playing
///         return true;
///     }
///
///     virtual void onStop() // optional
///     {
///         // Clean up whatever has to be done after the capture ends
///         ...
///     }
/// }
///
/// // Usage
/// if (CustomRecorder::isAvailable())
/// {
///     CustomRecorder recorder;
///
///     if (!recorder.start())
///         return -1;
///
///     ...
///     recorder.stop();
/// }
/// \endcode
///
/// \see sf::SoundBufferRecorder
///
////////////////////////////////////////////////////////////
