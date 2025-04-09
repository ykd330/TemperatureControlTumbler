adult <- read.table("C:\\Users\\Administrator\\Desktop\\Datafile\\adult.data")
names(aval) <- c("Sex", "Length", "Diameter", "Height", "Whole_weight", "Shucked_weight", "Viscera_weight", "Shell_weight", "Rings")
gapminder[gapminder$country=='Korea, Rep.', c('pop', 'gdpPercap')]
adult[1:3, ] %>%
  summarize(n_obs = n(),
            n_countries = n_distinct(country),
            n_years = n_distinct(year),
            med_gdpc = median(gdpPercap),
            max_gdppc = max(gdpPercap))
names(adult) <- c('age', 'workclass', 'fnlwgt', 'education','education_num', 'marital_status', 'occupation','relationship', 'race', 'sex','capital_gain', 'capital_loss','hours_per_week', 'native_country','wage')

g1 = housing %>% ggplot(aes(x = lstat)) + geom_histogram()
g2 = housing %>% ggplot(aes(x = medv)) + geom_histogram()
g3 = housing %>% ggplot(aes(x= medv)) + geom_histogram() + scale_x_log10()
g4 = housing %>% ggplot(aes(x= medv, y= lstat)) + geom_point() +scale_x_log10()+ geom_smooth()
grid.arrange(g1,g2,g3,g4,nrow=2,ncol=2)