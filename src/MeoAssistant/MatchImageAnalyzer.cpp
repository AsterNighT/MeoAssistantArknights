#include "MatchImageAnalyzer.h"

#include "AsstUtils.hpp"
#include "Logger.hpp"
#include "Resource.h"

asst::MatchImageAnalyzer::MatchImageAnalyzer(const cv::Mat image, const Rect& roi, std::string templ_name, double templ_thres)
    : AbstractImageAnalyzer(image, roi),
    m_templ_name(std::move(templ_name)),
    m_templ_thres(templ_thres)
{
    ;
}

bool asst::MatchImageAnalyzer::analyze()
{
    const cv::Mat templ = Resrc.templ().get_templ(m_templ_name);
    if (templ.empty()) {
        Log.error("templ is empty!");
        return false;
    }
    return match_templ(templ);
}

void asst::MatchImageAnalyzer::set_use_cache(bool is_use) noexcept
{
    m_use_cache = is_use;
}

void asst::MatchImageAnalyzer::set_mask_range(int lower, int upper) noexcept
{
    m_mask_range = std::make_pair(lower, upper);
}

void asst::MatchImageAnalyzer::set_mask_range(std::pair<int, int> mask_range) noexcept
{
    m_mask_range = std::move(mask_range);
}

void asst::MatchImageAnalyzer::set_templ_name(std::string templ_name) noexcept
{
    m_templ_name = std::move(templ_name);
}

void asst::MatchImageAnalyzer::set_threshold(double templ_thres) noexcept
{
    m_templ_thres = templ_thres;
}

void asst::MatchImageAnalyzer::set_task_info(std::shared_ptr<TaskInfo> task_ptr)
{
    set_task_info(*std::dynamic_pointer_cast<MatchTaskInfo>(task_ptr));
}

void asst::MatchImageAnalyzer::set_task_info(const std::string& task_name)
{
    set_task_info(Task.get(task_name));
}

const asst::MatchRect& asst::MatchImageAnalyzer::get_result() const noexcept
{
    return m_result;
}

void asst::MatchImageAnalyzer::set_task_info(MatchTaskInfo task_info) noexcept
{
    m_mask_range = std::move(task_info.mask_range);
    m_templ_name = std::move(task_info.templ_name);
    m_templ_thres = task_info.templ_threshold;

    if (task_info.cache && !task_info.region_of_appeared.empty()) {
        m_roi = task_info.region_of_appeared;
    }
    else {
        set_roi(task_info.roi);
        correct_roi();
    }
}
bool asst::MatchImageAnalyzer::match_templ(const cv::Mat templ)
{
    cv::Mat matched;

    cv::Mat image_roi = m_image(utils::make_rect<cv::Rect>(m_roi));
    if (templ.cols > image_roi.cols || templ.rows > image_roi.rows) {
        Log.error("templ size is too large", m_templ_name,
            "image_roi size:", image_roi.cols, image_roi.rows,
            "templ size:", templ.cols, templ.rows);
        return false;
    }
    if (m_mask_range.first == 0 && m_mask_range.second == 0) {
        cv::matchTemplate(image_roi, templ, matched, cv::TM_CCOEFF_NORMED);
    }
    else {
        cv::Mat mask;
        cv::cvtColor(templ, mask, cv::COLOR_BGR2GRAY);
        cv::inRange(mask, m_mask_range.first, m_mask_range.second, mask);
        cv::matchTemplate(image_roi, templ, matched, cv::TM_CCOEFF_NORMED, mask);
    }
    double min_val = 0.0, max_val = 0.0;
    cv::Point min_loc, max_loc;
    cv::minMaxLoc(matched, &min_val, &max_val, &min_loc, &max_loc);

    Rect rect(max_loc.x + m_roi.x, max_loc.y + m_roi.y, templ.cols, templ.rows);
    if (max_val > m_templ_thres * 0.7) { // 得分太低的肯定不对，没必要打印
        Log.trace("match_templ |", m_templ_name, "score:", max_val, "rect:", rect.to_string(), "roi:", m_roi.to_string());
    }

    if (max_val >= m_templ_thres) {
        m_result = { max_val, rect };
        return true;
    }
    else {
        return false;
    }
}
