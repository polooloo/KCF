#include "KCFTracker.h"
#include <cassert>
namespace SK
{
	KCFTracker::KCFTracker(bool hog /*= false*/, bool fixed_window /*= true*/, bool multiscale /*= false*/)
	{
		lambda = 0.0001f;
		padding = 2.5f;
		output_sigma_factor = 0.125f;
		if (hog)
		{
		}
		else
		{
			interp_factor = 0.075f;
			sigma = 0.2f;
			cell_size = 1;
			_hogfeatures = false;
		}

		if (multiscale)
		{
		}
		else if (fixed_window)
		{
			template_size = 96;
			scale_step = 1;
		}
		else
		{
			template_size = 96;
			scale_step = 1;
		}
	}

	KCFTracker::~KCFTracker()
	{
	}

	void KCFTracker::Init(Rect& roi, Mat image)
	{
		//m_roi = roi;
		RectCopy<int, float>(roi, m_roi);
		assert(roi.width > 0 && roi.height > 0);
		_tmpl = getFeatures(image, 1);

		fft_row = CFFT(_tmpl.cols());
		fft_col = CFFT(_tmpl.rows());

		createGaussianPeak(size_patch[0], size_patch[1], _prob);
		_alphaf = MatrixXcf(size_patch[0], size_patch[1]);
		_alphaf.fill(complexf(0));

		train(_tmpl, 1.0); // train with initial frame
	}

	Rect KCFTracker::update(Mat image)
	{
		if (m_roi.x + m_roi.width <= 0) m_roi.x = -m_roi.width + 1;
		if (m_roi.y + m_roi.height <= 0) m_roi.y = -m_roi.height + 1;
		if (m_roi.x >= image.cols() - 1) m_roi.x = image.cols() - 2;
		if (m_roi.y >= image.rows() - 1) m_roi.y = image.rows() - 2;

		float cx = m_roi.x + m_roi.width / 2.0f;
		float cy = m_roi.y + m_roi.height / 2.0f;

		float peak_value;
		Point2f res = detect(_tmpl, getFeatures(image, 0, 1.0f), peak_value);

		if (scale_step != 1)
		{
		}

		// Adjust by cell size and _scale
		m_roi.x = cx - m_roi.width / 2.0f + ((float)res.x * cell_size * _scale);
		m_roi.y = cy - m_roi.height / 2.0f + ((float)res.y * cell_size * _scale);

		if (m_roi.x >= image.cols() - 1) m_roi.x = image.cols() - 1;
		if (m_roi.y >= image.rows() - 1) m_roi.y = image.rows() - 1;
		if (m_roi.x + m_roi.width <= 0) m_roi.x = -m_roi.width + 2;
		if (m_roi.y + m_roi.height <= 0) m_roi.y = -m_roi.height + 2;

		assert(m_roi.width >= 0 && m_roi.height >= 0);
		MatrixXf x = getFeatures(image, 0);
		train(x, interp_factor);

		Rect roi;
		RectCopy<int, float>(roi, m_roi);
		return roi;
		//return Rect(1, 2, 3, 4);
	}

	Eigen::MatrixXf KCFTracker::getFeatures(const Mat& image, bool inithann, float scale_adjust /*= 1.0f*/)
	{
		Rect extracted_roi;

		//��ʵroi��������ģ�δpading֮ǰ��
		float cx = m_roi.x + m_roi.width * 0.5f; 
		float cy = m_roi.y + m_roi.height * 0.5f;

		if (inithann)
		{
			//����������ֻ�е�һ֡��ʼ����ʱ��ִ��
			int padded_w = static_cast<int>(m_roi.width) * padding;
			int padded_h = static_cast<int>(m_roi.height) * padding;

			if (template_size > 1)
			{
				//�跨�ƽ�������template size
				_scale = (padded_w >= padded_h) ? padded_w / (float)template_size :
					padded_h / (float)template_size;

				//�������ź�ĳߴ�
				_tmpl_sz.nWidth = static_cast<int>(padded_w / _scale);
				_tmpl_sz.nHeight = static_cast<int>(padded_h / _scale);
			}
			else
			{
				//û�и���template size�� ʹ�� ROI size
				_tmpl_sz.nWidth = padded_w;
				_tmpl_sz.nHeight = padded_h;
				_scale = 1;
			}
			if (_hogfeatures)
			{
			}
			else
			{
				// ȷ��������ż��
				_tmpl_sz.nWidth = (_tmpl_sz.nWidth / 2) * 2;
				_tmpl_sz.nHeight = (_tmpl_sz.nHeight / 2) * 2;
			}
		}

		//���յĽ������ Լ���� pading��ĳߴ磨��Ϊ��һ��ȷ��ż���Ĳ�����ȡ��������
		//Ϊʲô����ô�󾢣�����������ԭ����
		extracted_roi.width = scale_adjust * _scale * _tmpl_sz.nWidth;
		extracted_roi.height = scale_adjust * _scale * _tmpl_sz.nHeight;

		extracted_roi.x = cx - extracted_roi.width * 0.5;
		extracted_roi.y = cy - extracted_roi.height * 0.5;  // �������ĵ�û���ƶ�

		Eigen::MatrixXf FeaturesMap;
		Mat z = SK::subwindow(image, extracted_roi);
		if (z.cols() != _tmpl_sz.nWidth || z.rows() != _tmpl_sz.nHeight) {
			SK::Resize(z, _tmpl_sz);
		}
		if (_hogfeatures)
		{
		}
		else
		{
			FeaturesMap = getGrayImage(z);
			FeaturesMap.array() -= 0.5f;
			// size_patch[] ���������������: ����ֻ��һ���������ԣ�����˵ֻ��һ��ͨ����, ��Ϊz.cols�� ��Ϊz.rows
			size_patch[0] = z.rows();
			size_patch[1] = z.cols();
			size_patch[2] = 1;
		}
		if (inithann) {
			CreateHanningMats();
		}
		//FeaturesMap = hann.mul(FeaturesMap); opencv ��Ԫ�س˷�
		FeaturesMap = hann.cwiseProduct(FeaturesMap);
		return FeaturesMap;
	}

	void KCFTracker::CreateHanningMats()
	{
		MatrixXf hann1t(1, size_patch[1]); //���У� �У�
		hann1t.fill(0);
		MatrixXf hann2t(size_patch[0], 1); // (�У� ��)
		hann2t.fill(0);
		for (int i = 0; i < hann1t.cols(); i++)
		{
			hann1t(0, i) = 0.5f * (1 - std::cos(2 * 3.141593f * i / (hann1t.cols() - 1)));
		}
		for (int i = 0; i < hann2t.rows(); i++)
		{
			hann2t(i, 0) = 0.5f * (1 - std::cos(2 * 3.141593f * i / (hann2t.rows() - 1)));
		}

		MatrixXf hann2d = hann2t * hann1t;
		if (_hogfeatures)
		{
		}
		else
		{
			// Gray features
			hann = hann2d;
		}
	}

	float KCFTracker::subPixelPeak(float left, float center, float right)
	{
		float divisor = 2 * center - right - left;

		if (divisor == 0)
			return 0;

		return 0.5f * (right - left) / divisor;
	}

	Eigen::MatrixXf KCFTracker::gaussianCorrelation(MatrixXf x1, MatrixXf x2)
	{
		//cv::Mat c = cv::Mat(cv::Size(size_patch[1], size_patch[0]), CV_32F, cv::Scalar(0));
		MatrixXf c(size_patch[0], size_patch[1]);
		c.fill(0);
		if (_hogfeatures)
		{
		}
		else
		{
			//�Ҷ�����
			MatrixXf c(x1.rows(), x1.cols());

			MatrixXcf x1_out;
			MatrixXcf x2_out;
			FFT2D(x1, x1_out);
			FFT2D(x2, x2_out);

			iFFT2D(x1_out.cwiseProduct(x2_out.conjugate()), c);
			rearrange(c);
		}
		
		MatrixXf d;
		d = x1.cwiseProduct(x1).sum() + x2.cwiseProduct(x2).sum() - (c * 2).array();
		d = d / (size_patch[0] * size_patch[1] * size_patch[2]);
		d = (d.array() > 0).select(d, 0);
		
		MatrixXf k;
		//cv::exp((-d / (sigma * sigma)), k);
		
		k = (-d / (sigma * sigma)).array().exp();
		return k;
	}

	void KCFTracker::createGaussianPeak(int sizey, int sizex, MatrixXcf& resMatrix)
	{
		MatrixXf res(sizey, sizex);

		int syh = (sizey) / 2;
		int sxh = (sizex) / 2;

		float output_sigma = std::sqrt((float)sizex * sizey) / padding * output_sigma_factor; // ����
		float mult = -0.5f / (output_sigma * output_sigma);

		for (int i = 0; i < sizey; i++)
			for (int j = 0; j < sizex; j++)
			{
				int ih = i - syh;
				int jh = j - sxh;
				res(i, j) = std::exp(mult * (float)(ih * ih + jh * jh));
			}
		//MatrixXcf res_fft;
		//FFT2D(res, res_fft);
		//return res_fft;
		//resMatrix.resize(res.rows(), res.cols());
		FFT2D(res, resMatrix);
	}

	void KCFTracker::FFT2D(const MatrixXf& in, MatrixXcf& out)
	{
		out.resize(in.rows(), in.cols());

		for (int i = 0; i < in.rows(); i++)
		{
			//ÿһ�н���һάfft
			vector<complexf> in_row;
			vector<complexf> out_row;
			for (int j = 0; j < in.cols(); j++)
			{
				in_row.push_back(in(i, j));
			}
			fft_row.fft1D(in_row, out_row);
			for (size_t j = 0; j < out_row.size(); j++)
			{
				out(i, j) = out_row[j];
			}
		}

		for (int i = 0; i < out.cols(); i++)
		{
			//ÿһ�н���һάfft
			vector<complexf> in_col;
			vector<complexf> out_col;
			for (int j = 0; j < out.rows(); j++)
			{
				in_col.push_back(out(j, i));
			}
			fft_col.fft1D(in_col, out_col);
			for (size_t j = 0; j < out_col.size(); j++)
			{
				out(j, i) = out_col[j];
			}
		}
	}

	void KCFTracker::iFFT2D(const MatrixXcf& in, MatrixXf& out)
	{
		out.resize(in.rows(), in.cols());
		MatrixXcf outTmp(in.rows(), in.cols());

		for (int i = 0; i < in.rows(); i++)
		{
			//ÿһ�н���һάfft
			vector<complexf> in_row;
			vector<complexf> out_row;
			for (int j = 0; j < in.cols(); j++)
			{
				in_row.push_back(in(i, j));
			}
			fft_row.ifft1D(in_row, out_row);
			for (size_t j = 0; j < out_row.size(); j++)
			{
				outTmp(i, j) = out_row[j];
			}
		}

		for (int i = 0; i < out.cols(); i++)
		{
			//ÿһ�н���һάfft
			vector<complexf> in_col;
			vector<complexf> out_col;
			for (int j = 0; j < out.rows(); j++)
			{
				in_col.push_back(outTmp(j, i));
			}
			fft_col.ifft1D(in_col, out_col);
			for (size_t j = 0; j < out_col.size(); j++)
			{
				out(j, i) = out_col[j].real();
			}
		}
	}

	void KCFTracker::train(MatrixXf x, float train_interp_factor)
	{
		MatrixXf k = gaussianCorrelation(x, x);
		MatrixXcf  k_fft;
		FFT2D(k, k_fft);
		k_fft.array() += lambda;
		MatrixXcf alphaf = _prob.cwiseQuotient(k_fft);

		_tmpl = (1 - train_interp_factor) * _tmpl + (train_interp_factor)* x;
		_alphaf = (1 - train_interp_factor) * _alphaf + (train_interp_factor)* alphaf;

	}

	SK::Point2f KCFTracker::detect(MatrixXf z, MatrixXf x, float &peak_value)
	{
		MatrixXf k = gaussianCorrelation(x, z);
		//cv::Mat res = (real(fftd(complexMultiplication(_alphaf, fftd(k)), true)));
		MatrixXcf k_fft;
		FFT2D(k, k_fft);

		MatrixXf res;
		iFFT2D(_alphaf.cwiseProduct(k_fft.conjugate()), res);

		Point<int> pi;
		//float pv;
		int r = 0;
		int c = 0;
		peak_value = res.maxCoeff(&r, &c);


		//subpixel peak estimation, coordinates will be non-integer
		Point2f p((float)c, (float)r);

		if (pi.x > 0 && pi.x < res.cols() - 1) {
			p.x += subPixelPeak(res(pi.y, pi.x - 1), peak_value, res(pi.y, pi.x + 1));
		}

		if (pi.y > 0 && pi.y < res.rows() - 1) {
			p.y += subPixelPeak(res(pi.y - 1, pi.x), peak_value, res(pi.y + 1, pi.x));
		}

		p.x -= (res.cols()) / 2;
		p.y -= (res.rows()) / 2;

		return p;
	}

} // namespace SK