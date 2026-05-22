#include <QtWidgets>

class StereoCreatorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit StereoCreatorWindow(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("3D Moving Stereo Video Creator");
        setWindowIcon(QIcon(":/icons/app_icon.svg"));
        setMinimumSize(1280, 760);

        ffmpegPath = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpegPath.isEmpty() && QFileInfo::exists("D:/stereo/ffmpeg.exe")) {
            ffmpegPath = "D:/stereo/ffmpeg.exe";
        }

        buildUi();
        connectSignals();
        setControlsEnabled(false);
        updatePreviewImages(QPixmap(), QPixmap());

        previewTimer.setSingleShot(true);
        previewTimer.setInterval(140);
        connect(&previewTimer, &QTimer::timeout, this, &StereoCreatorWindow::updatePreviewNow);
    }

private:
    QLabel *fileLabel = nullptr;
    QLabel *leftImage = nullptr;
    QLabel *rightImage = nullptr;
    QSlider *slider = nullptr;
    QSpinBox *skipSpin = nullptr;
    QSpinBox *cropBottomSpin = nullptr;
    QDoubleSpinBox *trimStartSpin = nullptr;
    QDoubleSpinBox *trimEndSpin = nullptr;
    QPushButton *trimStartButton = nullptr;
    QPushButton *trimEndButton = nullptr;
    QLabel *bgmLabel = nullptr;
    QPushButton *bgmBrowseButton = nullptr;
    QPushButton *bgmClearButton = nullptr;
    QSpinBox *windowOffsetSpin = nullptr;
    QSpinBox *borderWidthSpin = nullptr;
    QPushButton *borderColorButton = nullptr;
    QPushButton *swapButton = nullptr;
    QPushButton *aboutButton = nullptr;
    QCheckBox *halfWidthCheck = nullptr;
    QCheckBox *anaglyphCheck = nullptr;
    QPushButton *startButton = nullptr;
    QProgressBar *progressBar = nullptr;
    QLabel *statusLabel = nullptr;

    QTimer previewTimer;
    QProcess *encodingProcess = nullptr;
    QTemporaryDir tempDir;

    QString inputPath;
    QString bgmPath;
    QString currentOutputPath;
    QString ffmpegPath;
    int totalFrames = 0;
    double fps = 30.0;
    double duration = 0.0;
    int originalWidth = 16;
    int originalHeight = 9;
    int displayWidth = 16;
    int displayHeight = 9;
    int rotationDegrees = 0;
    bool hasAudio = false;
    bool isSwapped = false;
    bool updatingBounds = false;
    QColor borderColor = Qt::black;
    int expectedOutputFrames = 0;
    QElapsedTimer encodeTimer;

    void buildUi()
    {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);

        auto *topLayout = new QHBoxLayout;
        fileLabel = new QLabel("Please select a video file to begin.");
        fileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *browseButton = new QPushButton("Browse...");
        topLayout->addWidget(fileLabel);
        topLayout->addWidget(browseButton);
        mainLayout->addLayout(topLayout);
        connect(browseButton, &QPushButton::clicked, this, &StereoCreatorWindow::selectFile);

        auto *previewLayout = new QHBoxLayout;
        previewLayout->setAlignment(Qt::AlignCenter);
        leftImage = makePreviewLabel();
        rightImage = makePreviewLabel();
        previewLayout->addWidget(leftImage);
        previewLayout->addWidget(rightImage);
        mainLayout->addLayout(previewLayout, 1);

        slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        mainLayout->addWidget(slider);

        auto *controlsLayout = new QGridLayout;
        controlsLayout->setHorizontalSpacing(10);
        controlsLayout->setVerticalSpacing(6);

        skipSpin = new QSpinBox;
        skipSpin->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        skipSpin->setRange(0, 1000000);
        skipSpin->setValue(5);
        skipSpin->setFixedWidth(76);

        cropBottomSpin = new QSpinBox;
        cropBottomSpin->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        cropBottomSpin->setRange(0, 1000000);
        cropBottomSpin->setFixedWidth(82);
        trimStartButton = new QPushButton("Cut from front");
        trimEndButton = new QPushButton("Cut from end");
        trimStartSpin = makeSecondsSpin();
        trimEndSpin = makeSecondsSpin();

        bgmLabel = new QLabel("No BGM audio selected.");
        bgmLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        bgmLabel->setMinimumWidth(260);
        bgmBrowseButton = new QPushButton("Browse BGM...");
        bgmClearButton = new QPushButton("Clear");

        auto *inwardButton = new QPushButton("< Inward");
        auto *outwardButton = new QPushButton("Outward >");
        windowOffsetSpin = new QSpinBox;
        windowOffsetSpin->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        windowOffsetSpin->setRange(-1000000, 1000000);
        windowOffsetSpin->setFixedWidth(72);
        connect(inwardButton, &QPushButton::clicked, this, [this] { windowOffsetSpin->setValue(windowOffsetSpin->value() - 5); });
        connect(outwardButton, &QPushButton::clicked, this, [this] { windowOffsetSpin->setValue(windowOffsetSpin->value() + 5); });

        borderWidthSpin = new QSpinBox;
        borderWidthSpin->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        borderWidthSpin->setRange(0, 500);
        borderWidthSpin->setFixedWidth(72);
        borderColorButton = new QPushButton("Color");
        borderColorButton->setFixedWidth(86);
        updateBorderColorButton();

        swapButton = new QPushButton("<-> Swap to Cross-Eye View");
        aboutButton = new QPushButton("About");
        halfWidthCheck = new QCheckBox("Half-Width SBS");
        anaglyphCheck = new QCheckBox("Anaglyph");
        startButton = new QPushButton("Create Stereo Video");
        QFont startFont = startButton->font();
        startFont.setBold(true);
        startFont.setPointSize(startFont.pointSize() + 1);
        startButton->setFont(startFont);
        startButton->setStyleSheet("QPushButton { background: #4CAF50; color: white; padding: 8px 18px; }");

        int col = 0;
        controlsLayout->addWidget(new QLabel("Skip:"), 0, col++, Qt::AlignRight);
        controlsLayout->addWidget(skipSpin, 0, col++);
        controlsLayout->addWidget(new QLabel("Crop bottom:"), 0, col++, Qt::AlignRight);
        controlsLayout->addWidget(cropBottomSpin, 0, col++);
        controlsLayout->addWidget(trimStartButton, 0, col++);
        controlsLayout->addWidget(trimStartSpin, 0, col++);
        controlsLayout->addWidget(trimEndButton, 0, col++);
        controlsLayout->addWidget(trimEndSpin, 0, col++);

        col = 0;
        controlsLayout->addWidget(new QLabel("Stereo window:"), 1, col++, Qt::AlignRight);
        controlsLayout->addWidget(inwardButton, 1, col++);
        controlsLayout->addWidget(windowOffsetSpin, 1, col++);
        controlsLayout->addWidget(outwardButton, 1, col++);
        controlsLayout->addWidget(new QLabel("Border width:"), 1, col++, Qt::AlignRight);
        controlsLayout->addWidget(borderWidthSpin, 1, col++);
        controlsLayout->addWidget(new QLabel("Border color:"), 1, col++, Qt::AlignRight);
        controlsLayout->addWidget(borderColorButton, 1, col++);
        controlsLayout->addWidget(swapButton, 1, col++, 1, 2);

        col = 0;
        controlsLayout->addWidget(new QLabel("BGM:"), 2, col++, Qt::AlignRight);
        controlsLayout->addWidget(bgmLabel, 2, col, 1, 4);
        col += 4;
        controlsLayout->addWidget(bgmBrowseButton, 2, col++);
        controlsLayout->addWidget(bgmClearButton, 2, col++);
        controlsLayout->addWidget(halfWidthCheck, 2, col++);
        controlsLayout->addWidget(anaglyphCheck, 2, col++);
        controlsLayout->addWidget(aboutButton, 2, col++);
        controlsLayout->addWidget(startButton, 2, col, 1, 2);
        col += 2;
        controlsLayout->setColumnStretch(1, 1);
        mainLayout->addLayout(controlsLayout);

        progressBar = new QProgressBar;
        progressBar->setRange(0, 100);
        statusLabel = new QLabel;
        statusLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(progressBar);
        mainLayout->addWidget(statusLabel);
    }

    QLabel *makePreviewLabel()
    {
        auto *label = new QLabel;
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("background: black;");
        label->setMinimumSize(200, 150);
        return label;
    }

    QDoubleSpinBox *makeSecondsSpin()
    {
        auto *spin = new QDoubleSpinBox;
        spin->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        spin->setRange(0.0, 10000000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.1);
        spin->setFixedWidth(110);
        return spin;
    }

    void connectSignals()
    {
        connect(slider, &QSlider::valueChanged, this, &StereoCreatorWindow::requestPreview);
        connect(skipSpin, &QSpinBox::valueChanged, this, &StereoCreatorWindow::onPreviewSettingChanged);
        connect(cropBottomSpin, &QSpinBox::valueChanged, this, &StereoCreatorWindow::onPreviewSettingChanged);
        connect(windowOffsetSpin, &QSpinBox::valueChanged, this, &StereoCreatorWindow::onPreviewSettingChanged);
        connect(borderWidthSpin, &QSpinBox::valueChanged, this, &StereoCreatorWindow::onPreviewSettingChanged);
        connect(trimStartSpin, &QDoubleSpinBox::valueChanged, this, &StereoCreatorWindow::onTrimChanged);
        connect(trimEndSpin, &QDoubleSpinBox::valueChanged, this, &StereoCreatorWindow::onTrimChanged);
        connect(trimStartButton, &QPushButton::clicked, this, &StereoCreatorWindow::setTrimStartFromSlider);
        connect(trimEndButton, &QPushButton::clicked, this, &StereoCreatorWindow::setTrimEndFromSlider);
        connect(bgmBrowseButton, &QPushButton::clicked, this, &StereoCreatorWindow::selectBgmFile);
        connect(bgmClearButton, &QPushButton::clicked, this, &StereoCreatorWindow::clearBgmFile);
        connect(borderColorButton, &QPushButton::clicked, this, &StereoCreatorWindow::selectBorderColor);
        connect(swapButton, &QPushButton::clicked, this, &StereoCreatorWindow::toggleSwap);
        connect(aboutButton, &QPushButton::clicked, this, &StereoCreatorWindow::showAbout);
        connect(anaglyphCheck, &QCheckBox::toggled, this, &StereoCreatorWindow::onAnaglyphToggled);
        connect(startButton, &QPushButton::clicked, this, &StereoCreatorWindow::startProcessing);
    }

    void setControlsEnabled(bool enabled)
    {
        slider->setEnabled(enabled);
        skipSpin->setEnabled(enabled);
        cropBottomSpin->setEnabled(enabled);
        borderWidthSpin->setEnabled(enabled);
        borderColorButton->setEnabled(enabled);
        trimStartSpin->setEnabled(enabled);
        trimEndSpin->setEnabled(enabled);
        trimStartButton->setEnabled(enabled);
        trimEndButton->setEnabled(enabled);
        startButton->setEnabled(enabled);
    }

    void selectBgmFile()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select BGM Audio File", QString(), "Audio Files (*.mp3 *.wav *.m4a *.aac *.flac *.ogg);;All files (*.*)");
        if (path.isEmpty()) {
            return;
        }
        bgmPath = path;
        bgmLabel->setText(QFileInfo(path).fileName());
    }

    void clearBgmFile()
    {
        bgmPath.clear();
        bgmLabel->setText("No BGM audio selected.");
    }

    void updateBorderColorButton()
    {
        const QString colorName = borderColor.name(QColor::HexRgb).toUpper();
        borderColorButton->setText(colorName);
        borderColorButton->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
            .arg(colorName)
            .arg(borderColor.lightness() < 128 ? "white" : "black"));
    }

    void selectBorderColor()
    {
        const QColor chosen = QColorDialog::getColor(borderColor, this, "Select Border Color");
        if (!chosen.isValid()) {
            return;
        }
        borderColor = chosen;
        updateBorderColorButton();
        requestPreview();
    }

    QString ffmpegColor(const QColor &color) const
    {
        return "0x" + color.name(QColor::HexRgb).mid(1).toUpper();
    }

    void onAnaglyphToggled(bool checked)
    {
        halfWidthCheck->setEnabled(!checked);
        if (checked) {
            halfWidthCheck->setChecked(false);
        }
        requestPreview();
    }

    void showAbout()
    {
        QMessageBox::about(
            this,
            "About 3D Moving Stereo Video Creator",
            "<h3>3D Moving Stereo Video Creator</h3>"
            "<p><b>Version:</b> 1.0<br>"
            "<b>Author:</b> Rathinagiri Subbiah</p>"
            "<p>This software converts suitable moving-camera video into side-by-side 3D stereo video by using two frames separated by a configurable frame skip.</p>"
            "<p>It is mainly intended for video captured from a moving train or car while keeping the camera steady, without horizontal shifting or tilting left or right. With stable forward or sideways motion, skipped frames can simulate a 3D camera effect when there are not many fast moving objects in the scene.</p>"
            "<p>The app supports frame skipping, stereo window adjustment, crop, trim, optional borders, parallel/cross-eye SBS, half-width SBS, red/cyan anaglyph output, view swapping, and optional BGM audio muxing.</p>"
            "<p><b>Requirement:</b> FFmpeg must be available in the system PATH. This build also tries <code>D:/stereo/ffmpeg.exe</code> as a fallback.</p>");
    }

    void selectFile()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select Video File", QString(), "Video Files (*.mp4 *.avi *.mov);;All files (*.*)");
        if (path.isEmpty()) {
            return;
        }
        if (ffmpegPath.isEmpty()) {
            QMessageBox::critical(this, "Error", "Could not find ffmpeg.exe. Add it to PATH or place it at D:/stereo/ffmpeg.exe.");
            return;
        }
        if (!probeVideo(path)) {
            QMessageBox::critical(this, "Error", "Could not read video metadata with FFmpeg.");
            return;
        }

        inputPath = path;
        fileLabel->setText(QFileInfo(path).fileName());
        cropBottomSpin->setRange(0, qMax(0, displayHeight - 3));
        trimStartSpin->setMaximum(duration);
        trimEndSpin->setMaximum(duration);
        setControlsEnabled(true);
        updateSliderBounds();
        requestPreview();
    }

    bool probeVideo(const QString &path)
    {
        QProcess probe;
        probe.setProgram(ffmpegPath);
        probe.setArguments({"-hide_banner", "-i", path});
        probe.setProcessChannelMode(QProcess::MergedChannels);
        probe.start();
        probe.waitForFinished(10000);
        const QString output = QString::fromUtf8(probe.readAll());

        QRegularExpression durationRe("Duration:\\s*(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)");
        QRegularExpression fpsRe("(\\d+(?:\\.\\d+)?)\\s*fps");
        QRegularExpression sizeRe("Video:.*?(\\d{2,5})x(\\d{2,5})");
        QRegularExpression rotateTagRe("rotate\\s*:\\s*(-?\\d+)");
        QRegularExpression displayMatrixRe("rotation of\\s*(-?\\d+(?:\\.\\d+)?)\\s*degrees");

        auto durationMatch = durationRe.match(output);
        auto fpsMatch = fpsRe.match(output);
        auto sizeMatch = sizeRe.match(output);
        if (!durationMatch.hasMatch() || !sizeMatch.hasMatch()) {
            return false;
        }

        const int hours = durationMatch.captured(1).toInt();
        const int minutes = durationMatch.captured(2).toInt();
        const double seconds = durationMatch.captured(3).toDouble();
        duration = hours * 3600.0 + minutes * 60.0 + seconds;
        fps = fpsMatch.hasMatch() ? fpsMatch.captured(1).toDouble() : 30.0;
        if (fps <= 0.0) {
            fps = 30.0;
        }
        originalWidth = sizeMatch.captured(1).toInt();
        originalHeight = qMax(1, sizeMatch.captured(2).toInt());
        rotationDegrees = 0;
        auto rotateTagMatch = rotateTagRe.match(output);
        auto displayMatrixMatch = displayMatrixRe.match(output);
        if (rotateTagMatch.hasMatch()) {
            rotationDegrees = rotateTagMatch.captured(1).toInt();
        } else if (displayMatrixMatch.hasMatch()) {
            rotationDegrees = static_cast<int>(std::round(displayMatrixMatch.captured(1).toDouble()));
        }
        rotationDegrees = ((rotationDegrees % 360) + 360) % 360;
        const bool rotatedSideways = rotationDegrees == 90 || rotationDegrees == 270;
        displayWidth = rotatedSideways ? originalHeight : originalWidth;
        displayHeight = rotatedSideways ? originalWidth : originalHeight;
        totalFrames = qMax(1, static_cast<int>(std::round(duration * fps)));
        hasAudio = output.contains("Audio:", Qt::CaseInsensitive);
        return true;
    }

    QSize previewDimensions() const
    {
        const int cropBottom = cropBottomSpin ? cropBottomSpin->value() : 0;
        const int offset = windowOffsetSpin ? qAbs(windowOffsetSpin->value()) : 0;
        const int borderWidth = borderWidthSpin ? borderWidthSpin->value() : 0;
        const int adjustedWidth = qMax(1, displayWidth - offset + borderWidth * 2);
        const int adjustedHeight = qMax(1, displayHeight - cropBottom + borderWidth * 2);
        const double aspect = static_cast<double>(adjustedWidth) / adjustedHeight;
        const int height1 = 600;
        const int width1 = static_cast<int>(height1 * aspect);
        int availableImageWidth = (anaglyphCheck && anaglyphCheck->isChecked()) ? width() - 50 : (width() / 2) - 25;
        availableImageWidth = qMax(1, availableImageWidth);
        const int width2 = availableImageWidth;
        const int height2 = static_cast<int>(width2 / aspect);
        return (height1 < height2) ? QSize(width1, height1) : QSize(width2, height2);
    }

    std::pair<int, int> trimFrameBounds() const
    {
        const int startFrame = qMin(static_cast<int>(std::round(trimStartSpin->value() * fps)), totalFrames);
        const int endFrame = qMax(totalFrames - static_cast<int>(std::round(trimEndSpin->value() * fps)), 0);
        return {startFrame, endFrame};
    }

    void updateSliderBounds()
    {
        if (inputPath.isEmpty()) {
            return;
        }
        const auto [trimStartFrame, trimEndFrame] = trimFrameBounds();
        const int maxLeftFrame = trimEndFrame - skipSpin->value() - 2;
        updatingBounds = true;
        if (maxLeftFrame < trimStartFrame) {
            slider->setRange(trimStartFrame, trimStartFrame);
            slider->setValue(trimStartFrame);
        } else {
            const int current = slider->value();
            slider->setRange(trimStartFrame, maxLeftFrame);
            slider->setValue(qBound(trimStartFrame, current, maxLeftFrame));
        }
        updatingBounds = false;
    }

    void requestPreview()
    {
        if (!updatingBounds) {
            previewTimer.start();
        }
    }

    void onPreviewSettingChanged()
    {
        updateSliderBounds();
        requestPreview();
    }

    void onTrimChanged()
    {
        updateSliderBounds();
        requestPreview();
    }

    void setTrimStartFromSlider()
    {
        if (!inputPath.isEmpty()) {
            trimStartSpin->setValue(slider->value() / fps);
        }
    }

    void setTrimEndFromSlider()
    {
        if (!inputPath.isEmpty()) {
            trimEndSpin->setValue(qMax(0.0, duration - (slider->value() / fps)));
        }
    }

    void toggleSwap()
    {
        isSwapped = !isSwapped;
        swapButton->setText(isSwapped ? "<-> Swap to Parallel View" : "<-> Swap to Cross-Eye View");
        requestPreview();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        requestPreview();
    }

    void updatePreviewNow()
    {
        if (inputPath.isEmpty()) {
            updatePreviewImages(QPixmap(), QPixmap());
            return;
        }
        const int leftFrame = slider->value();
        const int rightFrame = leftFrame + skipSpin->value() + 1;
        QPixmap left = framePixmap(leftFrame, true);
        QPixmap right = framePixmap(rightFrame, false);
        if (isSwapped) {
            updatePreviewImages(right, left);
        } else {
            updatePreviewImages(left, right);
        }
    }

    QPixmap framePixmap(int frameIndex, bool leftEye)
    {
        if (frameIndex < 0 || frameIndex >= totalFrames || !tempDir.isValid()) {
            return {};
        }

        const QString outPath = tempDir.filePath(QString("preview_%1_%2.png").arg(leftEye ? "l" : "r").arg(frameIndex));
        QFile::remove(outPath);

        QProcess extract;
        extract.setProgram(ffmpegPath);
        extract.setArguments({
            "-y",
            "-hide_banner",
            "-loglevel", "error",
            "-ss", QString::number(frameIndex / fps, 'f', 6),
            "-i", inputPath,
            "-frames:v", "1",
            outPath
        });
        extract.start();
        extract.waitForFinished(8000);

        QImage image(outPath);
        if (image.isNull()) {
            return {};
        }

        const int cropBottom = cropBottomSpin->value();
        if (cropBottom > 0 && cropBottom < image.height()) {
            image = image.copy(0, 0, image.width(), image.height() - cropBottom);
        }

        const int offset = windowOffsetSpin->value();
        const int sourceOffset = qMin(qAbs(offset), image.width() - 1);
        if (sourceOffset > 0) {
            if (offset < 0) {
                image = leftEye
                    ? image.copy(sourceOffset, 0, image.width() - sourceOffset, image.height())
                    : image.copy(0, 0, image.width() - sourceOffset, image.height());
            } else {
                image = leftEye
                    ? image.copy(0, 0, image.width() - sourceOffset, image.height())
                    : image.copy(sourceOffset, 0, image.width() - sourceOffset, image.height());
            }
        }

        const int borderWidth = borderWidthSpin->value();
        if (borderWidth > 0) {
            QImage bordered(image.width() + borderWidth * 2, image.height() + borderWidth * 2, image.format());
            bordered.fill(borderColor);
            QPainter painter(&bordered);
            painter.drawImage(borderWidth, borderWidth, image);
            image = bordered;
        }

        const QSize size = previewDimensions();
        image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        return QPixmap::fromImage(image);
    }

    void updatePreviewImages(const QPixmap &left, const QPixmap &right)
    {
        QPixmap placeholder(previewDimensions());
        placeholder.fill(Qt::black);
        if (anaglyphCheck && anaglyphCheck->isChecked()) {
            rightImage->setVisible(false);
            if (!left.isNull() && !right.isNull()) {
                leftImage->setPixmap(anaglyphPreview(left, right));
            } else {
                leftImage->setPixmap(placeholder);
            }
            return;
        }
        rightImage->setVisible(true);
        leftImage->setPixmap(left.isNull() ? placeholder : left);
        rightImage->setPixmap(right.isNull() ? placeholder : right);
    }

    QPixmap anaglyphPreview(const QPixmap &left, const QPixmap &right) const
    {
        QImage leftImageData = left.toImage().convertToFormat(QImage::Format_RGB32);
        QImage rightImageData = right.toImage().convertToFormat(QImage::Format_RGB32);
        if (leftImageData.size() != rightImageData.size()) {
            rightImageData = rightImageData.scaled(leftImageData.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        QImage output(leftImageData.size(), QImage::Format_RGB32);
        for (int y = 0; y < output.height(); ++y) {
            const QRgb *leftLine = reinterpret_cast<const QRgb *>(leftImageData.constScanLine(y));
            const QRgb *rightLine = reinterpret_cast<const QRgb *>(rightImageData.constScanLine(y));
            QRgb *outLine = reinterpret_cast<QRgb *>(output.scanLine(y));
            for (int x = 0; x < output.width(); ++x) {
                const int red = qGray(leftLine[x]);
                outLine[x] = qRgb(red, qGreen(rightLine[x]), qBlue(rightLine[x]));
            }
        }
        return QPixmap::fromImage(output);
    }

    void startProcessing()
    {
        if (inputPath.isEmpty()) {
            QMessageBox::critical(this, "Error", "Please select a video file first.");
            return;
        }
        if (cropBottomSpin->value() >= displayHeight - 2) {
            QMessageBox::critical(this, "Error", "Please enter a valid bottom crop in pixels. It must be less than the video height.");
            return;
        }

        const auto [trimStartFrame, trimEndFrame] = trimFrameBounds();
        expectedOutputFrames = trimEndFrame - trimStartFrame - (skipSpin->value() + 1);
        if (expectedOutputFrames <= 0) {
            QMessageBox::critical(this, "Error", "The trim settings leave too little video after applying frames to skip.");
            return;
        }

        startButton->setEnabled(false);
        startButton->setText("Processing...");
        progressBar->setValue(0);
        statusLabel->setText("Preparing FFmpeg command...");

        QStringList args = buildFfmpegArguments(trimStartFrame, trimEndFrame);
        encodingProcess = new QProcess(this);
        encodingProcess->setProgram(ffmpegPath);
        encodingProcess->setArguments(args);
        encodingProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(encodingProcess, &QProcess::readyReadStandardOutput, this, &StereoCreatorWindow::readEncodingOutput);
        connect(encodingProcess, &QProcess::finished, this, &StereoCreatorWindow::encodingFinished);
        encodeTimer.start();
        statusLabel->setText("Initializing FFmpeg...");
        encodingProcess->start();
    }

    QStringList buildFfmpegArguments(int trimStartFrame, int trimEndFrame)
    {
        const int framesToSkip = skipSpin->value();
        const int endFrameForLeft = trimEndFrame - (framesToSkip + 1);
        const int startFrameForRight = trimStartFrame + framesToSkip + 1;
        const int cropBottom = cropBottomSpin->value();
        const int offset = windowOffsetSpin->value();
        const int borderWidth = borderWidthSpin->value();
        const bool makeAnaglyph = anaglyphCheck->isChecked();
        const bool makeHalfWidth = halfWidthCheck->isChecked() && !makeAnaglyph;
        const QString borderColorValue = ffmpegColor(borderColor);

        auto videoChain = [&](const QString &label, int start, int end, bool leftEye) {
            QString chain = QString("[0:v]select='between(n,%1,%2)',setpts=PTS-STARTPTS").arg(start).arg(end - 1);
            if (cropBottom > 0) {
                chain += QString(",crop=iw:trunc((ih-%1)/2)*2:0:0").arg(cropBottom);
            }
            if (offset != 0) {
                const QString cropW = QString("trunc((iw-%1)/2)*2").arg(qAbs(offset));
                if (offset < 0) {
                    chain += leftEye
                        ? QString(",crop=%1:ih:%2:0").arg(cropW).arg(qAbs(offset))
                        : QString(",crop=%1:ih:0:0").arg(cropW);
                } else {
                    chain += leftEye
                        ? QString(",crop=%1:ih:0:0").arg(cropW)
                        : QString(",crop=%1:ih:%2:0").arg(cropW).arg(offset);
                }
            }
            if (makeHalfWidth) {
                chain += ",scale=iw/2:ih";
            }
            if (borderWidth > 0) {
                chain += QString(",pad=iw+%1:ih+%1:%2:%2:%3")
                    .arg(borderWidth * 2)
                    .arg(borderWidth)
                    .arg(borderColorValue);
            }
            chain += QString("[%1]").arg(label);
            return chain;
        };

        const bool useBgm = !bgmPath.isEmpty();
        const double bgmVolume = hasAudio ? 0.20 : 1.00;
        QString filter = videoChain("l", trimStartFrame, endFrameForLeft, true) + ";"
            + videoChain("r", startFrameForRight, trimEndFrame, false) + ";";
        if (makeAnaglyph) {
            filter += isSwapped
                ? "[r][l]hstack[sbs];[sbs]stereo3d=sbsl:arcd[v]"
                : "[l][r]hstack[sbs];[sbs]stereo3d=sbsl:arcd[v]";
        } else {
            filter += isSwapped ? "[r][l]hstack[v]" : "[l][r]hstack[v]";
        }

        const double audioDuration = expectedOutputFrames / fps;
        if (hasAudio) {
            const double audioStart = trimStartFrame / fps;
            filter += QString(";[0:a]atrim=start=%1:duration=%2,asetpts=PTS-STARTPTS[srca]")
                .arg(audioStart, 0, 'f', 6)
                .arg(audioDuration, 0, 'f', 6);
        }
        if (useBgm) {
            filter += QString(";[1:a]atrim=duration=%1,asetpts=PTS-STARTPTS,volume=%2[bgm]")
                .arg(audioDuration, 0, 'f', 6)
                .arg(bgmVolume, 0, 'f', 2);
        }
        if (hasAudio && useBgm) {
            filter += ";[srca][bgm]amix=inputs=2:duration=first:dropout_transition=0:normalize=0[a]";
        } else if (hasAudio) {
            filter += ";[srca]anull[a]";
        } else if (useBgm) {
            filter += ";[bgm]anull[a]";
        }

        const QFileInfo info(inputPath);
        const QString suffix = QString("%1%2%3%4%5%6")
            .arg(cropBottom > 0 ? QString("_CropB%1").arg(cropBottom) : QString())
            .arg((trimStartFrame || trimEndFrame != totalFrames) ? QString("_Trim%1-%2").arg(trimStartFrame).arg(trimEndFrame) : QString())
            .arg(borderWidth > 0 ? QString("_Border%1").arg(borderWidth) : QString())
            .arg(makeHalfWidth ? "_Half-SBS" : "")
            .arg(makeAnaglyph ? "_Anaglyph" : "")
            .arg(isSwapped ? "_X" : "");
        currentOutputPath = info.dir().filePath(info.completeBaseName() + "_3D_Stereo" + suffix + ".mp4");

        QStringList args = {
            "-progress", "pipe:1",
            "-y",
            "-i", inputPath
        };
        if (useBgm) {
            args << "-stream_loop" << "-1" << "-i" << bgmPath;
        }
        args << QStringList {
            "-filter_complex", filter,
            "-map", "[v]"
        };
        if (hasAudio || useBgm) {
            args << "-map" << "[a]" << "-c:a" << "aac";
        }
        args << "-c:v" << "libx264"
             << "-pix_fmt" << "yuv420p"
             << "-preset" << "ultrafast"
             << "-movflags" << "faststart"
             << currentOutputPath;
        return args;
    }

    void readEncodingOutput()
    {
        const QString output = QString::fromUtf8(encodingProcess->readAllStandardOutput());
        QRegularExpression frameRe("frame=\\s*(\\d+)");
        auto it = frameRe.globalMatch(output);
        while (it.hasNext()) {
            const int currentFrame = it.next().captured(1).toInt();
            if (currentFrame <= 0) {
                continue;
            }
            const int progress = qMin(100, static_cast<int>((currentFrame * 100.0) / expectedOutputFrames));
            const double elapsed = encodeTimer.elapsed() / 1000.0;
            const double currentFps = elapsed > 0.0 ? currentFrame / elapsed : 0.0;
            const int etr = currentFps > 0.0 ? static_cast<int>((expectedOutputFrames - currentFrame) / currentFps) : 0;
            progressBar->setValue(progress);
            statusLabel->setText(QString("Frame %1/%2 | ETR: %3m %4s")
                .arg(currentFrame)
                .arg(expectedOutputFrames)
                .arg(etr / 60)
                .arg(etr % 60));
        }
    }

    void encodingFinished(int exitCode, QProcess::ExitStatus exitStatus)
    {
        const QString remaining = QString::fromUtf8(encodingProcess->readAllStandardOutput());
        encodingProcess->deleteLater();
        encodingProcess = nullptr;

        startButton->setEnabled(true);
        startButton->setText("Create Stereo Video");

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            progressBar->setValue(100);
            statusLabel->setText("Success!");
            QMessageBox::information(this, "Success", "3D Video created:\n" + currentOutputPath);
        } else {
            statusLabel->setText("Process stopped.");
            QMessageBox::critical(this, "Error", "FFmpeg failed.\n\n" + remaining.right(2000));
        }
    }
};

int main(int argc, char *argv[])
{
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/app_icon.svg"));
    StereoCreatorWindow window;
    window.show();
    return app.exec();
}

#include "main.moc"
