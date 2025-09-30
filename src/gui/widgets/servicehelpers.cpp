#ifdef BUILD_GUI

#include "gui/widgets/servicehelpers.hpp"

namespace ak {
namespace gui {
namespace widgets {

namespace {

const std::vector<ServiceOption> kServiceOptions = {
    {QStringLiteral("OpenAI"), QStringLiteral("openai")},
    {QStringLiteral("Anthropic"), QStringLiteral("anthropic")},
    {QStringLiteral("Google"), QStringLiteral("gemini")},
    {QStringLiteral("Gemini"), QStringLiteral("gemini")},
    {QStringLiteral("Azure"), QStringLiteral("azure_openai")},
    {QStringLiteral("AWS"), QStringLiteral("aws")},
    {QStringLiteral("GitHub"), QStringLiteral("github")},
    {QStringLiteral("Slack"), QStringLiteral("slack")},
    {QStringLiteral("Discord"), QStringLiteral("discord")},
    {QStringLiteral("Stripe"), QStringLiteral("stripe")},
    {QStringLiteral("Twilio"), QStringLiteral("twilio")},
    {QStringLiteral("SendGrid"), QStringLiteral("sendgrid")},
    {QStringLiteral("Mailgun"), QStringLiteral("mailgun")},
    {QStringLiteral("Cloudflare"), QStringLiteral("cloudflare")},
    {QStringLiteral("Vercel"), QStringLiteral("vercel")},
    {QStringLiteral("Netlify"), QStringLiteral("netlify")},
    {QStringLiteral("Heroku"), QStringLiteral("heroku")},
    {QStringLiteral("Groq"), QStringLiteral("groq")},
    {QStringLiteral("Mistral"), QStringLiteral("mistral")},
    {QStringLiteral("Cohere"), QStringLiteral("cohere")},
    {QStringLiteral("Brave"), QStringLiteral("brave")},
    {QStringLiteral("DeepSeek"), QStringLiteral("deepseek")},
    {QStringLiteral("Exa"), QStringLiteral("exa")},
    {QStringLiteral("Fireworks"), QStringLiteral("fireworks")},
    {QStringLiteral("Hugging Face"), QStringLiteral("huggingface")},
    {QStringLiteral("OpenRouter"), QStringLiteral("openrouter")},
    {QStringLiteral("Perplexity"), QStringLiteral("perplexity")},
    {QStringLiteral("SambaNova"), QStringLiteral("sambanova")},
    {QStringLiteral("Tavily"), QStringLiteral("tavily")},
    {QStringLiteral("Together AI"), QStringLiteral("together")},
    {QStringLiteral("xAI"), QStringLiteral("xai")}
};

} // namespace

const std::vector<ServiceOption>& serviceOptions()
{
    return kServiceOptions;
}

QString serviceCodeForDisplay(const QString& displayName)
{
    for (const auto& option : kServiceOptions) {
        if (option.displayName.compare(displayName, Qt::CaseInsensitive) == 0) {
            return option.serviceCode;
        }
    }
    return QString();
}

QString displayForServiceCode(const QString& serviceCode)
{
    for (const auto& option : kServiceOptions) {
        if (option.serviceCode.compare(serviceCode, Qt::CaseInsensitive) == 0) {
            return option.displayName;
        }
    }
    return QString();
}

} // namespace widgets
} // namespace gui
} // namespace ak

#endif // BUILD_GUI

